cmake_minimum_required(VERSION 3.4.1)

# Compiler options
add_compile_options(-Wall)

# Submodule library
add_library(xmlstream
            STATIC

            # Source
            xml_ostream.c
            xml_istream.c)

# Linking
target_link_libraries(xmlstream
                      expat

                      # NDK libraries
                      log)
