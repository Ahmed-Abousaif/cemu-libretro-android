add_library(cemu_libretro SHARED
    CemuLibretro.cpp
    CemuLibretro.h
    Environment.cpp
    Environment.h
    LibretroAudio.cpp
    LibretroAudio.h
    LibretroInput.cpp
    LibretroInput.h
    LibretroVulkan.cpp
    LibretroVulkan.h
    Main.cpp
)

set_target_properties(cemu_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cemu_libretro"
)

cemu_use_precompiled_header(cemu_libretro)

target_link_libraries(cemu_libretro PRIVATE
    CemuCommon
    CemuAudio
    CemuComponents
    CemuCafe
    CemuBin
    CemuGui
    ZArchive::zarchive
    stb
)

if (ANDROID)
    target_link_libraries(cemu_libretro PRIVATE -landroid -llog)
    target_link_options(cemu_libretro PRIVATE "-fuse-ld=lld")
endif()
