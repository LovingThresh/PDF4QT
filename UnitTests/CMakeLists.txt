# MIT License
#
# Copyright (c) 2018-2025 Jakub Melka and Contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

add_executable(UnitTests
	tst_lexicalanalyzertest.cpp
)

target_link_libraries(UnitTests PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTests PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

# Agent LLM Client Tests
add_executable(PDFAgentUnitTests
    tst_pdfagentllmclient.cpp
)

target_link_libraries(PDFAgentUnitTests PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(PDFAgentUnitTests PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

# Agent Function Registry Tests
add_executable(PDFAgentFunctionRegistryTests
    tst_pdfagentfunctionregistry.cpp
)

target_link_libraries(PDFAgentFunctionRegistryTests PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(PDFAgentFunctionRegistryTests PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

# Agent Mock Tool Parser Tests
add_executable(PDFAgentMockToolParserTests
    tst_pdfagentmocktoolparser.cpp
)

target_link_libraries(PDFAgentMockToolParserTests PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(PDFAgentMockToolParserTests PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    set(WINDEPLOYQT_EXECUTABLE "${_qt_bin_dir}/windeployqt.exe")

    add_custom_command(TARGET PDFAgentUnitTests POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:PDFAgentUnitTests>"
            COMMENT "Running windeployqt to copy Qt DLLs..."
    )

    add_custom_command(TARGET PDFAgentFunctionRegistryTests POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:PDFAgentFunctionRegistryTests>"
            COMMENT "Running windeployqt for PDFAgentFunctionRegistryTests..."
    )

    add_custom_command(TARGET PDFAgentMockToolParserTests POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:PDFAgentMockToolParserTests>"
            COMMENT "Running windeployqt for PDFAgentMockToolParserTests..."
    )
endif()