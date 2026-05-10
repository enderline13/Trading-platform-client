cmake_minimum_required(VERSION 3.16)

project(Trading_Client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Charts Protobuf Grpc ProtobufWellKnownTypes)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts Protobuf Grpc ProtobufWellKnownTypes)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Trading_Client
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        tradingclient.h tradingclient.cpp
        accountclient.h accountclient.cpp
        marketclient.h marketclient.cpp
        authclient.h authclient.cpp
        adminclient.h adminclient.cpp
        proto_utils.h
        logindialog.h logindialog.cpp logindialog.ui
    )
endif()

qt_add_protobuf(Trading_Client
    PROTO_FILES
        proto/account.proto
        proto/admin.proto
        proto/auth.proto
        proto/common.proto
        proto/market.proto
        proto/trading.proto
)

qt_add_grpc(Trading_Client CLIENT
    PROTO_FILES
        proto/account.proto
        proto/admin.proto
        proto/auth.proto
        proto/common.proto
        proto/market.proto
        proto/trading.proto
)

target_link_libraries(Trading_Client PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Charts Qt${QT_VERSION_MAJOR}::Protobuf Qt${QT_VERSION_MAJOR}::Grpc Qt${QT_VERSION_MAJOR}::ProtobufWellKnownTypes)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Trading_Client)
endif()
set_target_properties(Trading_Client PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Trading_Client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Trading_Client)
endif()
