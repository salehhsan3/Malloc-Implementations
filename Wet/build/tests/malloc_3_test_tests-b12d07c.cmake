add_test( [==[malloc_3.Challenge 0 - Memory Utilization]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[Challenge 0 - Memory Utilization]==]  )
set_tests_properties( [==[malloc_3.Challenge 0 - Memory Utilization]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.test all sizes]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[test all sizes]==]  )
set_tests_properties( [==[malloc_3.test all sizes]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.Finding buddies test]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[Finding buddies test]==]  )
set_tests_properties( [==[malloc_3.Finding buddies test]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.multiple big allocs test]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[multiple big allocs test]==]  )
set_tests_properties( [==[malloc_3.multiple big allocs test]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.Exit Test]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[Exit Test]==]  )
set_tests_properties( [==[malloc_3.Exit Test]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.srealloc test]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[srealloc test]==]  )
set_tests_properties( [==[malloc_3.srealloc test]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.scalloc test]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[scalloc test]==]  )
set_tests_properties( [==[malloc_3.scalloc test]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.srealloc merges test]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[srealloc merges test]==]  )
set_tests_properties( [==[malloc_3.srealloc merges test]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_3.weird values]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_3_test [==[weird values]==]  )
set_tests_properties( [==[malloc_3.weird values]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
set( malloc_3_test_TESTS [==[malloc_3.Challenge 0 - Memory Utilization]==] [==[malloc_3.test all sizes]==] [==[malloc_3.Finding buddies test]==] [==[malloc_3.multiple big allocs test]==] [==[malloc_3.Exit Test]==] [==[malloc_3.srealloc test]==] [==[malloc_3.scalloc test]==] [==[malloc_3.srealloc merges test]==] [==[malloc_3.weird values]==])
