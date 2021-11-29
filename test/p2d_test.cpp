#include <iostream>
#include <chrono>
#include <memory>
#include <thread>

#include <p2d/p2d.hpp>

void wait(double ms) {
  auto d = std::chrono::duration<double, std::milli>(ms);
  std::this_thread::sleep_for(d);
}


int main(int argc, char **argv) {
  if (argc != 1) {
    std::cout << argv[0] << " takes no arguments.\n";
    return 1;
  }

  auto display = std::make_unique<p2d::Display>();
  auto driver = p2d::Driver();

  auto result = driver.Open(display);
  if (result.Failed()) {
    std::cout << result.GetDescription() << "\n";
  }

  wait(1000);

  return 0;
}
