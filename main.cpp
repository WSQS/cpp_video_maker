#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using uint32 = uint32_t;
using uint8 = uint8_t;

constexpr auto width = 800;
constexpr auto height = 600;

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
  for (int i = 0; i < framerate * 10; ++i) {
    raster.clear(get_color(11, 23, 58, 0xFF));
    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; ++y) {
        // pixels[y * width + x] = get_color(i, i, i, 0xFF);
      }
    }
    write(pipefd[WRITE_END], raster.get_pixels(),
          sizeof(uint32) * width * height);
  }
  close(pipefd[WRITE_END]);
  wait(nullptr);
  std::cout << "Finish\n";
  return 0;
}
