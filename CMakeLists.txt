cmake_minimum_required(VERSION 3.20)
project(picks_model VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PICKS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(PICKS_WERROR "Treat compiler warnings as errors" OFF)
option(PICKS_BUILD_TESTS "Build the unit tests" ON)

function(picks_setup target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
    if(PICKS_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
                                             -Wold-style-cast -Woverloaded-virtual
                                             -Wno-missing-field-initializers)
    if(PICKS_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(MINGW)
      # Bundle the C++ runtime so the .exe runs without the toolchain on PATH.
      target_link_options(${target} PRIVATE -static)
    endif()
    if(PICKS_SANITIZE)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
                                               -fno-sanitize-recover=all)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

add_library(picks_core
  src/backtest.cpp
  src/baselines.cpp
  src/config.cpp
  src/csv.cpp
  src/elo.cpp
  src/game.cpp
  src/goals.cpp
  src/history.cpp
  src/io.cpp
  src/json.cpp
  src/metrics.cpp
  src/predict.cpp
  src/strings.cpp
  src/time.cpp
)
target_include_directories(picks_core PUBLIC include)
target_compile_definitions(picks_core PUBLIC PICKS_VERSION="${PROJECT_VERSION}")
picks_setup(picks_core)

add_executable(picks-model src/main.cpp)
target_link_libraries(picks-model PRIVATE picks_core)
picks_setup(picks-model)

if(PICKS_BUILD_TESTS)
  enable_testing()
  add_executable(picks_tests
    tests/main.cpp
    tests/backtest_test.cpp
    tests/config_test.cpp
    tests/csv_test.cpp
    tests/elo_test.cpp
    tests/goals_test.cpp
    tests/history_test.cpp
    tests/json_test.cpp
    tests/metrics_test.cpp
    tests/predict_test.cpp
    tests/time_test.cpp
  )
  target_link_libraries(picks_tests PRIVATE picks_core)
  picks_setup(picks_tests)
  add_test(NAME unit COMMAND picks_tests)

  # End-to-end runs of the CLI on a tiny checked-in data set.
  set(sample ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
  add_test(NAME cli_backtest
    COMMAND picks-model backtest --history ${sample}/history --sport nba --warmup-days 0 --min-games 0)
  add_test(NAME cli_predict
    COMMAND picks-model predict --history ${sample}/history --fixtures ${sample}/fixtures.json
            --out ${CMAKE_CURRENT_BINARY_DIR}/model_picks.json --now 2026-01-01T00:00:00Z --min-games 1)
  add_test(NAME cli_ratings
    COMMAND picks-model ratings --history ${sample}/history --sport nba --min-games 1)
endif()
