add_executable(ragcpp_cli main.cpp)
set_target_properties(ragcpp_cli PROPERTIES OUTPUT_NAME ragcpp)

# Link the optional RCP front-end when available.
if(TARGET ragcpp::rcp)
    target_link_libraries(ragcpp_cli PRIVATE ragcpp::rcp)
else()
    target_link_libraries(ragcpp_cli PRIVATE ragcpp)
endif()


# ============================================================
# Enterprise Knowledge Gateway
# ============================================================

add_executable(
    ragcpp_enterprise_server
    enterprise_server.cpp
)

# Enterprise server uses ragcpp core directly.
# It does NOT require the optional RCP SDK.
target_link_libraries(
    ragcpp_enterprise_server
    PRIVATE
    ragcpp
)

# Windows networking
if(WIN32)
    target_link_libraries(
        ragcpp_enterprise_server
        PRIVATE
        ws2_32
    )
endif()
