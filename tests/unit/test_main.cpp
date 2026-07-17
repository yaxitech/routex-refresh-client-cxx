#include <catch2/catch_test_macros.hpp>

#include <yaxi/routex-refresh-client.h>

TEST_CASE("public type aliases are usable", "[smoke]") {
    yaxi::refresh::ConnectionId id = "connection-96386142-60e5-4ca9-abcf-944efce5bc1e";
    REQUIRE(id == "connection-96386142-60e5-4ca9-abcf-944efce5bc1e");

    yaxi::refresh::ConnectionData connectionData{0x01, 0x02, 0x03};
    REQUIRE(connectionData.size() == 3);

    yaxi::refresh::Session session{0x04, 0x05};
    REQUIRE(session.size() == 2);
}
