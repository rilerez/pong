#include <vector>
#include <algorithm>
#include <complex>
#include <random>
#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>

#include "sdl2raii/emscripten_glue.hpp"
#include "sdl2raii/sdl.hpp"

template<class Thunk>
struct finally {
  Thunk thunk;
  finally(Thunk thunk) : thunk{std::move(thunk)} {}
  ~finally() { thunk(); }
};

using namespace std::literals;
namespace chrono = std::chrono;

auto constexpr update_step = 20ms;

template<class N>
inline auto clamp(N const low, N const high, N const x) {
  using std::max;
  using std::min;
  return min(max(low, x), high);
}
#define FN(...)                                                                \
  (auto _) { return __VA_ARGS__; }

int const screen_height = 1024;
int const screen_width = 768;

struct Player {
  Player() = default;
  static int const margin = 80;
  static int constexpr width = 100;
  static int const height = 20;

  static int const y = screen_height - margin;

  float x = 180.0;
  float target = x;
  static float constexpr speed = 1.5;

  static float constexpr fingerradius = .2 * width;

  auto moveTo(float x) {
    auto const halfwidth = width / 2;
    auto const midx = this->x + halfwidth;
    if(x < midx - fingerradius)
      return -1;
    else if(x > midx + fingerradius)
      return 1;
    else
      return 0;
  }

  float vel() { return speed * moveTo(target); }

  auto moved_x(chrono::milliseconds dt) {
    return clamp<float>(0,
                        screen_width - width,
                        x + vel() * static_cast<float>(dt.count()));
  }

  auto rect(chrono::milliseconds lag = 0ms) {
    return sdl::Rect{static_cast<int>(moved_x(lag)), y, width, height};
  }
} player;

#include <complex>
using vec = std::complex<float>;
struct Ball {
  Ball() = default;
  static int const side = 20;

  vec p = {300, 300};
  vec v = {-.2, -.2};

  Ball(vec p) : p{std::move(p)} {}

  auto moved_p(chrono::milliseconds dt) {
    auto const newp = p + v * static_cast<float>(dt.count());
    auto const [x, y] = std::tuple{newp.real(), newp.imag()};
    auto const clamper = [](float const x, float const max) {
      return clamp<float>(0, max - side, x);
    };
    return vec{clamper(x, screen_width), clamper(y, screen_height)};
  }

  auto rect(chrono::milliseconds lag = 0ms) {
    auto const pnext = moved_p(lag);
    return sdl::Rect{static_cast<int>(pnext.real()),
                     static_cast<int>(pnext.imag()),
                     side,
                     side};
  }

  void flip_x() { v = {-v.real(), v.imag()}; }
  void flip_y() { v = {v.real(), -v.imag()}; }

} ball;

std::vector<sdl::Rect> bricks = {};

void update() {
  auto ballRect = ball.rect();

  player.x = player.moved_x(update_step);
  ball.p = ball.moved_p(update_step);

  auto const ballFlip = [](sdl::Rect r) {
    auto const walkBack = [] { ball.p = ball.moved_p(-2 * update_step); };
    auto const isPortrait = [] FN(_.h > _.w);
    if(r.h == r.w)
      r = *sdl::IntersectRect(
          r,
          Ball{ball.p + ball.v * -.1f * static_cast<float>(update_step.count())}
              .rect());
    walkBack();
    if(isPortrait(r))
      ball.flip_x();
    else
      ball.flip_y();
  };

  if(auto const intersectPlayer = sdl::IntersectRect(ballRect, player.rect()))
    ballFlip(*intersectPlayer);

  if(ball.p.imag() <= 0)
    ball.flip_y();
  if(ball.p.real() <= 0 || ball.p.real() >= screen_width - ball.side)
    ball.flip_x();
  auto shouldBreakBrick = [=] FN(sdl::HasIntersection(_, ballRect));
  auto collisionIter =
      partition(bricks.begin(), bricks.end(), [=] FN(!shouldBreakBrick(_)));
  if(collisionIter != bricks.end()) {
    auto const intersectBrick = *sdl::IntersectRect(ballRect, *collisionIter);
    ballFlip(intersectBrick);
    bricks.erase(collisionIter, bricks.end());
  }
}

void render(sdl::Renderer* render, chrono::milliseconds lag) {
  sdl::SetRenderDrawColor(render, {80, 80, 80, 255});
  sdl::RenderClear(render);
  sdl::SetRenderDrawColor(render, {200, 200, 200, 255});
  sdl::RenderFillRect(render, player.rect(lag));
  sdl::RenderFillRect(render, ball.rect(lag));
  sdl::SetRenderDrawColor(render, {200, 100, 200, 255});
  sdl::RenderFillRects(render, &bricks[0], bricks.size());
  sdl::SetRenderDrawColor(render, {80, 80, 80, 255});
  sdl::RenderDrawRects(render, &bricks[0], bricks.size());
  sdl::RenderPresent(render);
}

void reset() {
  player = {};
  ball = {};
  auto cols = 12;
  bricks.clear();
  bricks.reserve(4);
  for(int r = 0; r < 3; ++r)
    for(int i = 0; i < cols; ++i)
      bricks.push_back(sdl::Rect{80 + 50 * i, 100 + 30 * r, 50, 30});
}

int main() {
  sdl::Init(sdl::init::video);
  finally _ = [] { sdl::Quit(); };

  auto window = sdl::CreateWindow("breakout",
                                  sdl::window::pos_undefined,
                                  sdl::window::pos_undefined,
                                  screen_width,
                                  screen_height,
                                  0);
  auto renderer = sdl::CreateRenderer(
      window.get(),
      -1,
      sdl::renderer::accelerated | sdl::renderer::presentvsync);

  reset();

  auto last_time = chrono::high_resolution_clock::now();
  auto lag = last_time - last_time;

  emscripten_glue::main_loop([&] {
    auto this_time = chrono::high_resolution_clock::now();
    auto elapsed_time = this_time - last_time;
    lag += elapsed_time;

    for(; lag >= update_step; lag -= update_step)
      update();

    while(auto const event = sdl::NextEvent()) {
      switch(event->type) {
        case SDL_QUIT:
          emscripten_glue::cancel_main_loop();
          break;
        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
          player.target = (event->tfinger.x) * static_cast<float>(screen_width);
          break;
        case SDL_KEYDOWN:
          switch(event->key.keysym.sym) {
            case SDLK_r:
              reset();
              break;
          }
      }
    }

    render(renderer.get(), chrono::duration_cast<chrono::milliseconds>(lag));

    last_time = this_time;
  });
  return 0;
}
