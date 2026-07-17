// In-process bootstrap test, mirroring the Kotlin
// AccountsAndBalancesAndTransactionsOnlineTest: drive the *interactive* client to
// establish a live ConnectionData, then verify the non-interactive refresh client
// reproduces the same payloads from it.
//
// Both clients coexist in one program because their public APIs live in different
// namespaces: the interactive client in `yaxi`, the refresh client in
// `yaxi::refresh`. The interactive client is fetched and linked by CMake on all
// platforms (see tests/unit/CMakeLists.txt).
#include "helpers.hpp"
#include "ticket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <yaxi/routex-client.h>
#include <yaxi/routex-refresh-client.h>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

using json = nlohmann::json;
using namespace yaxi::test;

namespace {
constexpr auto DEMO_CONNECTION_ID = "connection-96386142-60e5-4ca9-abcf-944efce5bc1e";
constexpr auto DEMO_IBAN = "DE02120300000000202051";

// The demo "result" user returns a result immediately (no dialog/redirect).
yaxi::ServiceResult requireInteractiveResult(yaxi::Result<yaxi::ServiceResponse> &result) {
    REQUIRE_THAT(result, HoldsAlternative<yaxi::ServiceResponse>());
    auto &response = std::get<yaxi::ServiceResponse>(result);
    REQUIRE_THAT(response, HoldsAlternative<yaxi::ServiceResult>());
    return std::get<yaxi::ServiceResult>(std::move(response));
}

yaxi::refresh::ServiceResult
requireRefreshResult(yaxi::refresh::Result<yaxi::refresh::ServiceResult> &result) {
    REQUIRE_THAT(result, HoldsAlternative<yaxi::refresh::ServiceResult>());
    return std::get<yaxi::refresh::ServiceResult>(std::move(result));
}
} // namespace

TEST_CASE("refresh client reproduces interactive results", "[online][refresh]") {
    auto cfg = getConfigOrSkip();

    yaxi::RoutexClient interactive(cfg.url);
    yaxi::refresh::RoutexRefreshClient refresh(cfg.url);
    TicketGenerator tickets(cfg.keyId, cfg.apiKeyBase64);

    const std::vector<yaxi::AccountField> interactiveFields{
        yaxi::AccountField::Iban,
        yaxi::AccountField::Currency,
        yaxi::AccountField::OwnerName,
    };
    const std::vector<yaxi::refresh::AccountField> refreshFields{
        yaxi::refresh::AccountField::Iban,
        yaxi::refresh::AccountField::Currency,
        yaxi::refresh::AccountField::OwnerName,
    };

    // ── Phase 1: interactive accounts establishes a reusable ConnectionData ──
    yaxi::Credentials credentials{};
    credentials.connectionId = DEMO_CONNECTION_ID;
    credentials.userId = "result";

    auto interactiveAccountsResp =
        interactive.accounts(credentials, tickets.accounts(uuidV4()), interactiveFields);
    auto interactiveAccounts = requireInteractiveResult(interactiveAccountsResp);

    REQUIRE(interactiveAccounts.connectionData.has_value());
    auto connectionData = *interactiveAccounts.connectionData;

    // The interactive result is a signed JWT whose "data"."data" claim is the
    // service output; the refresh result is that same output already decoded.
    auto interactiveAccountsData =
        jwtDecodeUnverified(interactiveAccounts.jwt).at("data").at("data");

    // ── Phase 2: refresh client replays the call from the ConnectionData ──
    auto refreshAccountsResp =
        refresh.accounts(connectionData, tickets.accounts(uuidV4()), refreshFields);
    auto refreshAccounts = requireRefreshResult(refreshAccountsResp);
    REQUIRE(json::parse(refreshAccounts.json) == interactiveAccountsData);

    // ── Balances through both paths, reusing the bootstrapped ConnectionData ──
    std::vector<yaxi::AccountReference> interactiveBalAccounts{
        yaxi::AccountReference{.iban = DEMO_IBAN, .currency = std::string{"EUR"}},
    };
    auto interactiveBalCreds = credentials;
    interactiveBalCreds.connectionData = connectionData;
    auto interactiveBalResp = interactive.balances(interactiveBalCreds, tickets.balances(uuidV4()),
                                                   interactiveBalAccounts);
    auto interactiveBalances = requireInteractiveResult(interactiveBalResp);
    auto interactiveBalancesData =
        jwtDecodeUnverified(interactiveBalances.jwt).at("data").at("data");

    std::vector<yaxi::refresh::AccountReference> refreshBalAccounts{
        yaxi::refresh::AccountReference{.iban = DEMO_IBAN, .currency = std::string{"EUR"}},
    };
    auto refreshBalResp =
        refresh.balances(connectionData, tickets.balances(uuidV4()), refreshBalAccounts);
    auto refreshBalances = requireRefreshResult(refreshBalResp);

    auto dropDateTimes = [](json balances) {
        for (auto &account : balances.at("balances")) {
            for (auto &balance : account.at("balances")) {
                balance.erase("dateTime");
            }
        }
        return balances;
    };
    REQUIRE(dropDateTimes(json::parse(refreshBalances.json)) ==
            dropDateTimes(interactiveBalancesData));
}
