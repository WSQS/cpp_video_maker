#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using uint32 = uint32_t;
using uint8 = uint_fast8_t;

constexpr auto width = 800;
constexpr auto height = 800;

class pipe_wrapper {
    constexpr static auto READ_END = 0;
    constexpr static auto WRITE_END = 1;
    enum class pipe_statues : uint8 { unavailable, available, read, write } statues{};
    int pipe_des[2]{};

public:
    pipe_wrapper() {
        if (pipe(pipe_des)) {
            std::cerr << "ERROR: pipe failed: " << strerror(errno) << std::endl;
            statues = pipe_statues::unavailable;
        } else {
            statues = pipe_statues::available;
        }
    }
    ~pipe_wrapper() {
        switch (statues) {
        case pipe_statues::available:
            close(pipe_des[READ_END]);
            close(pipe_des[WRITE_END]);
            break;
        case pipe_statues::read:
            close(pipe_des[READ_END]);
            break;
        case pipe_statues::write:
            close(pipe_des[WRITE_END]);
            break;
        default:
            break;
        }
    }
    bool valid() const { return statues != pipe_statues::unavailable; }
    auto read_end() {
        if (statues == pipe_statues::available) {
            statues = pipe_statues::read;
            close(pipe_des[WRITE_END]);
        }
        if (statues == pipe_statues::read)
            return pipe_des[READ_END];
        return -1;
    }
    auto write_end() {
        if (statues == pipe_statues::available) {
            statues = pipe_statues::write;
            close(pipe_des[READ_END]);
        }
        if (statues == pipe_statues::write)
            return pipe_des[WRITE_END];
        return -1;
    }
};

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
    // if (x < 0 || x >= width || y < 0 || y >= height)
    //   return;
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
  int size{};
  uint32 color{};
  void update() {
    pos = pos + velocity;
    pos.x = clamp(pos.x, size, width - size);
    pos.y = clamp(pos.y, size, height - size);
  }
  void render(Raster &raster) {
    for (int x = pos.x - size; x < pos.x + size; ++x) {
      for (int y = pos.y - size; y < pos.y + size; ++y) {
        raster.set_color(x, y, color);
      }
    }
  }
  void handle_collision() {
    if (pos.x <= size || pos.x >= width - size) {
      velocity.x = -velocity.x;
    }
    if (pos.y <= size || pos.y >= height - size) {
      velocity.y = -velocity.y;
    }
  }
};

struct Circle {
  vector pos{};
  vector velocity{};
  int size{};
  uint32 color{};
  void update() {
    pos = pos + velocity;
    pos.x = clamp(pos.x, size, width - size);
    pos.y = clamp(pos.y, size, height - size);
  }
  void render(Raster &raster) {
    for (int x = pos.x - size; x < pos.x + size; ++x) {
      for (int y = pos.y - size; y < pos.y + size; ++y) {
        if (std::pow(pos.x - x, 2) + std::pow(pos.y - y, 2) < std::pow(size, 2))
          raster.set_color(x, y, color);
      }
    }
  }
  void handle_collision() {
    if (pos.x <= size || pos.x >= width - size) {
      velocity.x = -velocity.x;
    }
    if (pos.y <= size || pos.y >= height - size) {
      velocity.y = -velocity.y;
    }
  }
};

int main() {
  const auto resolution = std::to_string(width) + "x" + std::to_string(height);
  constexpr auto framerate = 10;
  pipe_wrapper pipe_des;
  if (!pipe_des.valid()) {
    return 1;
  }
  std::cout << "hello world" << std::endl;
  auto pid = fork();
  if (!pid) {
    dup2(pipe_des.read_end(), STDIN_FILENO);
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
  Raster raster{};
  constexpr auto box_num = 1;
  std::vector<Circle> boxs(box_num);
  for (size_t i = 0; i < box_num; ++i) {
    boxs.emplace_back(
        Circle{{rand(0, width), rand(0, height)},
               {rand(-10, 10), rand(-10, 10)},
               rand(20, 30),
               get_color(std::rand(), std::rand(), std::rand(), std::rand())});
  }
  for (int i = 0; i < framerate * 10; ++i) {
    // raster.clear(get_color(11, 23, 58, 0xFF));
    for (auto &box : boxs) {
      box.update();
      box.render(raster);
      box =
          Circle{{rand(0, width), rand(0, height)},
                 {rand(-10, 10), rand(-10, 10)},
                 rand(20, 30),
                 get_color(std::rand(), std::rand(), std::rand(), std::rand())};

      // boxs.emplace_back(
      //     Box{{rand(0, width), rand(0, height)},
      //         {rand(-10, 10), rand(-10, 10)},
      //         rand(20, 30),
      //         get_color(std::rand(), std::rand(), std::rand(),
      //         std::rand())});

      box.handle_collision();
      // for (auto &box_obj : boxs) {
      //   if (abs(box.pos.x - box_obj.pos.x) < box.box_size + box_obj.box_size
      //   &&
      //       abs(box.pos.y - box_obj.pos.y) < box.box_size + box_obj.box_size)
      //       {
      //     if ((box_obj.pos.x - box.pos.x) *
      //             (box_obj.velocity.x - box.velocity.x) <
      //         0) {
      //       // auto temp = box.velocity.x * box.box_size;
      //       // auto temp_obj = box_obj.velocity.x * box_obj.box_size;
      //       // box.velocity.x = temp_obj / box.box_size;
      //       // box_obj.velocity.x = temp / box_obj.box_size;
      //       std::swap(box.velocity.x, box_obj.velocity.x);
      //     }
      //     if ((box_obj.pos.y - box.pos.y) *
      //             (box_obj.velocity.y - box.velocity.y) <
      //         0) {
      //       // auto temp = box.velocity.y * box.box_size;
      //       // auto temp_obj = box_obj.velocity.y * box_obj.box_size;
      //       // box.velocity.y = temp_obj / box.box_size;
      //       // box_obj.velocity.y = temp / box_obj.box_size;
      //       std::swap(box.velocity.y, box_obj.velocity.y);
      //     }
      //   }
      // }
    }
    write(pipe_des.write_end(), raster.get_pixels(),
          sizeof(uint32) * width * height);
  }
  close(pipe_des.write_end());
  wait(nullptr);
  std::cout << "Finish\n";
  return 0;
}
