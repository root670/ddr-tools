CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

project(tcbtools)

#
# Libraries
#
file(GLOB sources_pngquant
    pngquant/*.c
)
add_library(pngquant ${sources_pngquant})

file(GLOB sources_lodepng
    lodepng/*.c
)
add_library(lodepng ${sources_lodepng})

file(GLOB sources_bemanilz
    BemaniLZ/*.c
)
add_library(bemanilz ${sources_bemanilz})

#
# Executables
#
file(GLOB sources_tcb_extract
    tcb-extract.c
)
add_executable(tcb-extract ${sources_tcb_extract})
target_link_libraries(tcb-extract bemanilz)

file(GLOB sources_tcb_convert
    tcb-convert.c
)
add_executable(tcb-convert ${sources_tcb_convert})
target_link_libraries(tcb-convert bemanilz pngquant lodepng)