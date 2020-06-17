set(ASM_DIALECT "_JWASM")
set(CMAKE_ASM${ASM_DIALECT}_SOURCE_FILE_EXTENSIONS asm)

set(CMAKE_ASM${ASM_DIALECT}_COMPILE_OBJECT "<CMAKE_ASM${ASM_DIALECT}_COMPILER> -Cp -Zm -Zv8 -9 -coff -Fo <OBJECT> <SOURCE>")

include(CMakeASMInformation)
set(ASM_DIALECT)