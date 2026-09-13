cmake_minimum_required(VERSION 3.16)

# vcpkg toolchain'i (ZeroMQ/cppzmq icin) -- SADECE Windows'ta varsayilan
# olarak veriliyor (Qt Creator yeni bir build klasoru actiginda elle
# ayarlamak gerekmesin diye); -DCMAKE_TOOLCHAIN_FILE=... ile disaridan
# verilirse o ezer (NOT DEFINED kontrolu). Linux/Ubuntu'da ZeroMQ/cppzmq
# sistem paketinden (apt: libzmq3-dev + cppzmq) geldigi icin vcpkg'e hic
# gerek yok -- WIN32 kontrolu olmadan bu varsayilan Linux'ta var olmayan
# bir yolu CMAKE_TOOLCHAIN_FILE'a yazip derlemeyi kirardi.
if(WIN32)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "C:/Users/habib/Desktop/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    endif()
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        # ZeroMQ/cppzmq sadece bu triplet icin kurulu (vcpkg install ... --triplet
        # x64-mingw-dynamic) -- vcpkg varsayilani MSVC hedefi (x64-windows) sectigi
        # icin MinGW derleyicisiyle ZeroMQ'yu bulamiyordu.
        set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
    endif()
endif()

project(EHARPP VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PrintSupport)

# Telemetri artik 915 MHz seri radyo yerine ZeroMQ (loopback) uzerinden geliyor
# -- bkz. zmqsubscriber.h/zmqcommandpublisher.h. vcpkg ile kurulum:
#   vcpkg install zeromq cppzmq --triplet x64-mingw-dynamic
# ve CMake'e -DCMAKE_TOOLCHAIN_FILE=<vcpkg-kok>/scripts/buildsystems/vcpkg.cmake verilmeli.
find_package(ZeroMQ CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    modeswitch.cpp
    modeswitch.h
    qcustomplot.cpp
    qcustomplot.h
    system.h
    resources.qrc
    zmqsubscriber.cpp
    zmqsubscriber.h
    zmqcommandpublisher.cpp
    zmqcommandpublisher.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(EHARPP
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET EHARPP APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(EHARPP SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(EHARPP
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(EHARPP PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    libzmq
    cppzmq
)

# qcustomplot.cpp (~30k satır) Debug modda MinGW/COFF'un "too many sections"
# sınırını aşıyor -- büyük nesne dosyaları için binutils'in kendi çözümü.
if(MINGW)
    set_source_files_properties(qcustomplot.cpp PROPERTIES COMPILE_OPTIONS "-Wa,-mbig-obj")
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.EHARPP)
endif()
set_target_properties(EHARPP PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS EHARPP
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(EHARPP)
endif()
