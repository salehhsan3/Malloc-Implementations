add_test( malloc_1.Sanity /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_1_test Sanity  )
set_tests_properties( malloc_1.Sanity PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_1.Check size]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_1_test [==[Check size]==]  )
set_tests_properties( [==[malloc_1.Check size]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_1.0 size]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_1_test [==[0 size]==]  )
set_tests_properties( [==[malloc_1.0 size]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
add_test( [==[malloc_1.Max size]==] /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests/malloc_1_test [==[Max size]==]  )
set_tests_properties( [==[malloc_1.Max size]==] PROPERTIES WORKING_DIRECTORY /mnt/c/Users/Saleh/Desktop/OS/Spring/OS_HW4_SPRING23/Wet/build/tests)
set( malloc_1_test_TESTS malloc_1.Sanity [==[malloc_1.Check size]==] [==[malloc_1.0 size]==] [==[malloc_1.Max size]==])
