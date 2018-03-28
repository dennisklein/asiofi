/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char** argv)
{
  const size_t page = sysconf(_SC_PAGESIZE);

  auto bm = [&](benchmark::State& state){
    const size_t msg = state.range(0);
    const size_t step = msg / page;
    const size_t step2 = step / sizeof(int);

    for (auto _ : state) {
      auto x = static_cast<int*>(std::malloc(msg));
      benchmark::DoNotOptimize(x);
      auto x2 = x;
      for (size_t i = 0; i < msg; i += step) {
        *x2 = 42;
        benchmark::DoNotOptimize(x2);
        x2 += step2;
      }
      std::free(x);
    }
    state.SetBytesProcessed(state.iterations() * msg);
  };
  
  benchmark::RegisterBenchmark("malloc, page write, free", bm)->
    RangeMultiplier(2)->Range(1<<12, 1<<30)->Threads(1);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
