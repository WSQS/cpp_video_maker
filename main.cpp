#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using uint32 = uint32_t;
using uint8 = uint8_t;

constexpr auto width = 800;
constexpr auto height = 600;

inline int rand(int min, int max) {
  if (max < min) {
    std::cerr << __FUNCTION__ << " max and min is out of order\n";
    std::swap(min, max);
  }
  return std::rand() % (max - min) + min;
}

inline auto clamp(const auto &input, const auto &min, const auto &max) {
  return std::min(std::max(input, min), max);
}

inline constexpr uint32 get_color(uint8 r, uint8 g, uint8 b, uint8 a) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(r));
}

class Raster {
  uint32 pixels[width * height];

public:
  inline void clear(uint32 background) {
    for (int i = 0; i < width * height; ++i) {
      pixels[i] = background;
    }
  }
  const uint32 *get_pixels() const { return pixels; }
  void set_color(int x, int y, uint32 color) {
    if (x < 0 || x >= width || y < 0 || y >= height)
      return;
    pixels[y * width + x] = color;
  }
};

struct vector {
  int x{};
  int y{};
  vector operator+(const vector &input_vector) {
    return {x + input_vector.x, y + input_vector.y};
  }
};

struct Box {
  vector pos{};
  vector velocity{};
  int box_size{};
  uint32 color{};
  void update() {
    pos = pos + velocity;
    pos.x = clamp(pos.x, box_size, width - box_size);
    pos.y = clamp(pos.y, box_size, height - box_size);
  }
  void render(Raster &raster) {
    for (int x = pos.x - box_size; x < pos.x + box_size; ++x) {
      for (int y = pos.y - box_size; y < pos.y + box_size; ++y) {
        raster.set_color(x, y, color);
      }
    }
  }
  void handle_collision() {
    if (pos.x <= box_size || pos.x >= width - box_size) {
      velocity.x = -velocity.x;
    }
    if (pos.y <= box_size || pos.y >= height - box_size) {
      velocity.y = -velocity.y;
    }
  }
};

int main() {
  const auto resolution = std::to_string(width) + "x" + std::to_string(height);
  constexpr auto framerate = 60;
  constexpr auto READ_END = 0;
  constexpr auto WRITE_END = 1;
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    std::cerr << "ERROR: could not create a pipe: " << strerror(errno)
              << std::endl;
    return 1;
  }
  std::cout << "hello world" << std::endl;
  auto pid = fork();
  if (!pid) {
    dup2(pipefd[READ_END], STDIN_FILENO);
    close(pipefd[WRITE_END]);
    std::cout << pid << std::endl;
    int ret =
        execlp("ffmpeg", "ffmpeg",
               //"-loglevel", "verbose",
               "-y",

               "-f", "rawvideo", "-pix_fmt", "rgba", "-s", resolution.data(),
               "-r", std::to_string(framerate).data(), "-i", "-",

               "-c:v", "libx264", "output.mp4",

               NULL);
    if (ret < 0) {
      std::cerr << "ERROR: could not run ffmpeg as a child process: "
                << strerror(errno);
      exit(1);
    }
  }
  close(pipefd[READ_END]);
  Raster raster{};
  std::vector<Box> boxs(10);
  for (size_t i = 0; i < 10; ++i) {
    boxs.emplace_back(
        Box{{rand(0, width), rand(0, height)},
            {rand(-10, 10), rand(-10, 10)},
            100,
            get_color(std::rand(), std::rand(), std::rand(), std::rand())});
  }
  for (int i = 0; i < framerate * 10; ++i) {
    raster.clear(get_color(11, 23, 58, 0xFF));
    for (auto &box : boxs) {
      box.update();
      box.render(raster);
      box.handle_collision();
    }
    write(pipefd[WRITE_END], raster.get_pixels(),
          sizeof(uint32) * width * height);
  }
  close(pipefd[WRITE_END]);
  wait(nullptr);
  std::cout << "Finish\n";
  return 0;
}
