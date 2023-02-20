#include <iostream>
using namespace std;

#include "rsp-interface.hpp"

int main() {
   load();
   power(false);
   run();
   unload();

   cout << "Hello World" << endl;
   return 0;
}

