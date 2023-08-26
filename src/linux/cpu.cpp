// Copyright (c) Leon Freist <freist@informatik.uni-freiburg.de>
// This software is part of HWBenchmark

#include <hwinfo/platform.h>

#ifdef HWINFO_UNIX

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>

#include "hwinfo/cpu.h"
#include "hwinfo/utils/stringutils.h"
#include "hwinfo/utils/filesystem.h"

namespace hwinfo {

// _____________________________________________________________________________________________________________________
int64_t getMaxClockSpeed_MHz(const int& core_id) {
  int64_t Hz;
  filesystem::get_specs_by_file_path("/sys/devices/system/cpu/cpu" + std::to_string(core_id) + "/cpufreq/scaling_max_freq", Hz);
  if (Hz > -1)
  {
    return Hz / 1000;
  }

  return -1;
}

// _____________________________________________________________________________________________________________________
int64_t getRegularClockSpeed_MHz(const int& core_id) {
  int64_t Hz;
  filesystem::get_specs_by_file_path("/sys/devices/system/cpu/cpu" + std::to_string(core_id) + "/cpufreq/base_frequency", Hz);
  if (Hz > -1)
  {
    return Hz / 1000;
  }

  return -1;
}

int64_t getMinClockSpeed_MHz(const int& core_id) {
  int64_t Hz;
  filesystem::get_specs_by_file_path("/sys/devices/system/cpu/cpu" + std::to_string(core_id) + "/cpufreq/scaling_min_freq", Hz); 
  if (Hz > -1)
  {
    return Hz / 1000;
  }

  return -1;
}

// _____________________________________________________________________________________________________________________
int64_t CPU::currentClockSpeed_MHz() const {
  int64_t Hz;
  filesystem::get_specs_by_file_path("/sys/devices/system/cpu/cpu" + std::to_string(_core_id) + "/cpufreq/scaling_cur_freq", Hz);
  if (Hz > -1)
  {
    return Hz / 1000;
  }

  return -1;
 }

double CPU::currentLoadPercentage() const {
  if (_last_sum_all_jiffies == -1 || _last_sum_work_jiffies == -1)
  {
    filesystem::get_jiffies(_last_sum_all_jiffies, _last_sum_work_jiffies);
    usleep(1000 * 1000); // 1s
  }

  int64_t sum_all_jiffies;
  int64_t sum_work_jiffies;
  filesystem::get_jiffies(sum_all_jiffies, sum_work_jiffies);

  double total_over_period = sum_all_jiffies - _last_sum_all_jiffies;
  double work_over_period = sum_work_jiffies - _last_sum_work_jiffies;

  _last_sum_all_jiffies = sum_all_jiffies;
  _last_sum_work_jiffies = sum_work_jiffies;

  return work_over_period / total_over_period * 100.0;
}

// =====================================================================================================================
// _____________________________________________________________________________________________________________________
std::vector<Socket> getAllSockets() {
  std::vector<Socket> sockets;

  std::ifstream cpuinfo("/proc/cpuinfo");
  if (!cpuinfo.is_open()) {
    return {};
  }
  std::string file((std::istreambuf_iterator<char>(cpuinfo)), (std::istreambuf_iterator<char>()));
  cpuinfo.close();
  auto cpu_blocks_string = utils::split(file, "\n\n");
  std::map<const std::string, const std::string> cpu_block;
  int physical_id = -1;
  bool next_add = false;
  for (const auto& block : cpu_blocks_string) {
    CPU cpu;
    auto lines = utils::split(block, '\n');
    for (auto& line : lines) {
      auto line_pairs = utils::split(line, ":");
      if (line_pairs.size() < 2) {
        continue;
      }
      auto name = line_pairs[0];
      auto value = line_pairs[1];
      utils::strip(name);
      utils::strip(value);
      if (name == "processor") {
        cpu._core_id = std::stoi(value);
      } else if (name == "vendor_id") {
        cpu._vendor = value;
      } else if (name == "model name") {
        cpu._modelName = value;
      } else if (name == "cache size") {
        cpu._cacheSize_Bytes = std::stoi(utils::split(value, " ")[0]) * 1024;
      } else if (name == "physical id") {
        if (physical_id == std::stoi(value)) {
          continue;
        }
        next_add = true;
      } else if (name == "siblings") {
        cpu._numLogicalCores = std::stoi(value);
      } else if (name == "cpu cores") {
        cpu._numPhysicalCores = std::stoi(value);
      } else if (name == "flags") {
        cpu._flags = utils::split(value, " ");
      }
    }
    if (next_add) {
      cpu._maxClockSpeed_MHz = getMaxClockSpeed_MHz(cpu._core_id);
      cpu._minClockSpeed_MHz = getMinClockSpeed_MHz(cpu._core_id);
      cpu._regularClockSpeed_MHz = getRegularClockSpeed_MHz(cpu._core_id);
      cpu._last_sum_all_jiffies = -1;
      cpu._last_sum_work_jiffies = -1;
      cpu.currentLoadPercentage();
      next_add = false;
      Socket socket(cpu);
      physical_id++;
      socket._id = physical_id;
      sockets.push_back(std::move(socket));
    }
  }

  return sockets;
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX
