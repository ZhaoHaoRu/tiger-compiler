#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "straightline/prog1.h"

int main(int argc, char **argv) {
  int args;
  int test_num;

  assert(argc == 2);
  test_num = atoi(argv[1]);

  switch (test_num) {
  case 0:
    args = Prog()->MaxArgs();
    Prog()->Interp(nullptr);
    args = ProgProg()->MaxArgs();
    ProgProg()->Interp(nullptr);
    break;
  case 1:
    args = ProgProg()->MaxArgs();
    ProgProg()->Interp(nullptr);

    args = Prog()->MaxArgs();
    Prog()->Interp(nullptr);
    break;
  default:
    exit(-1);
  }
  args = RightProg()->MaxArgs();
  RightProg()->Interp(nullptr);

  return 0;
}
