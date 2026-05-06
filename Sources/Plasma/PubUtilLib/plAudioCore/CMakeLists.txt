set(plAudioCore_SOURCES
    plAudioFileReader.cpp
    plBufferedFileReader.cpp
    plCachedFileReader.cpp
    plFastWavReader.cpp
    plMP3Codec.cpp
    plOGGCodec.cpp
    plSoundBuffer.cpp
    plSoundDeswizzler.cpp
    plSrtFileReader.cpp
    plWavFile.cpp
)

set(plAudioCore_HEADERS
    plAudioCore.h
    plAudioCoreCreatable.h
    plAudioFileReader.h
    plBufferedFileReader.h
    plCachedFileReader.h
    plFastWavReader.h
    plMP3Codec.h
    plOGGCodec.h
    plSoundBuffer.h
    plSoundDeswizzler.h
    plSrtFileReader.h
    plWavFile.h
)

plasma_library(plAudioCore
    FOLDER PubUtilLib
    SOURCES ${plAudioCore_SOURCES} ${plAudioCore_HEADERS}
)
target_link_libraries(
    plAudioCore
    PUBLIC
        CoreLib
        pnKeyedObject
    PRIVATE
        pnMessage
        pnNucleusInc

        $<$<PLATFORM_ID:Windows>:${DirectX_LIBRARIES}>
        Ogg::ogg
        Vorbis::vorbis
        Vorbis::vorbisfile
        $<$<BOOL:${USE_MPG123}>:MPG123::libmpg123>
        $<$<PLATFORM_ID:Windows>:winmm>
    INTERFACE
        pnFactory
)

source_group("Source Files" FILES ${plAudioCore_SOURCES})
source_group("Header Files" FILES ${plAudioCore_HEADERS})
