//--------------------------------------------------------------------------------------------------
// System Programming                     Homework #2                                    Fall 2021
//
/// @file
/// @brief main file
/// @author Ryu Junyul
/// @studid 2016-17097
#include <stdio.h>
#include <stdlib.h>
#include "mathlib.h"

/// @brief program entry point
int main(int argc, char *argv[])
{
  // first print all command line arguments
  printf("Command line arguments (%d total):\n", argc);

  int sum = 0;

  for(int i = 0; i < argc; i++) {
    printf("   %d: '%s'\n", i, argv[i]);
    int val = atoi(argv[i]);
    sum = add(sum, val);
  }

  printf("\nSum of arguments: %d.\n", sum);

  return EXIT_SUCCESS;
}
