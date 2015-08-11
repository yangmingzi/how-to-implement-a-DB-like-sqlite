/*
  The test method is described below:
    1.Insert the same data into a B+Tree and a map.
    2.Read datas from B+Tree and the map by its key, and check if they
      are same.
*/

#include "BPlusTree.h"

#include <time.h>
#include <map>

/*
  Structure used to test
*/
struct Test {
  Test() {  // initialize a, b, c respectively and randomly.
    static int MOD = 1000000000;

    a = rand() % MOD;

    int b0 = rand() % MOD, b1 = rand() % MOD, t = rand() % 10;
    b = b0 + (double)b1 / t;

    c = rand() % ((long long)MOD * MOD);
  }

  bool friend operator == (const Test &t0, const Test &t1) {
    return t0.a == t1.a && t0.b == t1.b && t0.c == t1.c;
  }

  int a;
  double b;
  long long c;
};

int test() {
  srand(time(0));

  int test_case = 300000;

  Tree *root = getTree();

  static std::map<int, Test> test_map;
  test_map.clear();

  // insertion phase
  for (int i = 0; i < test_case; ++i) {
    int key = rand() % (test_case / 4 * 3);

    // ignore duplicate key
    if (test_map.find(key) != test_map.end()) continue;
    
    Test t;
    test_map[key] = t;
    if (!insert(root, key, &t, sizeof(Test))) return -1;
  }

  // check phase
  for (int i = 0; i < (test_case * 3 / 2); ++i) {
    int key = rand() % (test_case / 4 * 3);
    int ret_len;
    void *ret_p;
    static Test ret_data;

    if (test_map.find(key) == test_map.end()) {  // not found
      if (search(root, key, ret_p, ret_len)) return 1;
    } else {  // found in test_map
      if (!search(root, key, ret_p, ret_len)) return 2;
      memcpy(&ret_data, ret_p, ret_len);
      if (!(ret_data == test_map[key])) return 3;
    }
  }

  return 0;
}

int main() {
  printf("%d\n", test());
  return 0;
}
