#pragma once

#include "ticket.hpp"

#include <catch2/matchers/catch_matchers_templated.hpp>
#include <nameof.hpp>
#include <nlohmann/json.hpp>
#include <yaxi/routex-refresh-client.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace yaxi::test {

struct Config {
    std::string keyId;
    std::string apiKeyBase64;
    std::string url;
};

std::optional<Config> getConfig();

// Returns the loaded Config, or SKIPs the current Catch2 test if any of
// YAXI_API_KEY / YAXI_KEY_ID / ROUTEX_URL is missing.
Config getConfigOrSkip();

// Bootstraps connection data by running the interactive client against the
// demo connection (the non-interactive client reuses an existing connection
// data). Credential gating is the caller's job (OnlineFixture); this never
// skips, a failure to obtain the blob is a test failure.
::yaxi::refresh::ConnectionData bootstrapConnectionData();

nlohmann::json jwtDecodeUnverified(const std::string &jwt);

// HTTP GET that does NOT follow redirects. Returns the Location header value.
// Throws if the response status is not 303 or libcurl reports an error.
std::string httpGetRedirectLocation(const std::string &url);

std::string uuidV4();

std::vector<uint8_t> base64Decode(const std::string &input);

template <typename Expected>
class HoldsAlternativeMatcher : public Catch::Matchers::MatcherGenericBase {
  public:
    template <typename V> bool match(const V &v) const {
        return std::holds_alternative<Expected>(v);
    }

    std::string describe() const override {
        return "expected to be alternative " + std::string{nameof::nameof_type<Expected>()};
    }
};

template <typename T> HoldsAlternativeMatcher<T> HoldsAlternative() { return {}; }

// Fixture for [online] tests. The default ctor calls getConfigOrSkip(), so
// any test using TEST_CASE_METHOD(OnlineFixture, ...) is automatically
// skipped when YAXI_API_KEY / YAXI_KEY_ID / ROUTEX_URL is unset.
class OnlineFixture {
  public:
    OnlineFixture() : OnlineFixture(getConfigOrSkip()) {}

  protected:
    ::yaxi::refresh::RoutexRefreshClient client;
    TicketGenerator generator;

  private:
    explicit OnlineFixture(Config cfg) : client(cfg.url), generator(cfg.keyId, cfg.apiKeyBase64) {}
};

} // namespace yaxi::test
