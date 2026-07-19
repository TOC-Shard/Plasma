set(pfSurface_SOURCES
    plDistOpacityMod.cpp
    plFadeOpacityLay.cpp
    plFadeOpacityMod.cpp
    plGrabCubeMap.cpp
    plLayerAVI.cpp
    plLayerMovie.cpp
    plLayerWebM.cpp
)

set(pfSurface_HEADERS
    pfSurfaceCreatable.h
    plDistOpacityMod.h
    plFadeOpacityLay.h
    plFadeOpacityMod.h
    plGrabCubeMap.h
    plLayerAVI.h
    plLayerMovie.h
    plLayerWebM.h
    plLayerWebM_AVX2.h
)

plasma_library(pfSurface
    FOLDER FeatureLib
    SOURCES ${pfSurface_SOURCES} ${pfSurface_HEADERS}
    UNITY_BUILD
    PRECOMPILED_HEADERS Pch.h
)
# GCC/Clang (unlike MSVC) need the whole translation unit compiled with -mavx2
# before AVX2 intrinsics can be used at all -- see plLayerWebM_AVX2.h's comment.
plasma_target_simd_sources(pfSurface AVX2 plLayerWebM_AVX2.cpp)
target_link_libraries(
    pfSurface
    PUBLIC
        CoreLib
        pnModifier
        plSurface
    PRIVATE
        pnNucleusInc
        pnSceneObject
        plAudio
        plAvatar
        plDrawable
        plGImage
        plMessage
        plScene
        $<$<PLATFORM_ID:Windows>:vfw32>
        $<$<BOOL:${USE_WEBM}>:libwebm::libwebm>
        $<$<BOOL:${USE_WEBM}>:Opus::opus>
        $<$<BOOL:${USE_WEBM}>:VPX::VPX>
    INTERFACE
        pnFactory
)

source_group("Source Files" FILES ${pfSurface_SOURCES})
source_group("Header Files" FILES ${pfSurface_HEADERS})
