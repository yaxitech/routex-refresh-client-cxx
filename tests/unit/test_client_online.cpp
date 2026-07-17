#include "helpers.hpp"
#include "ticket.hpp"

#include <catch2/catch_test_macros.hpp>
#include <yaxi/routex-refresh-client.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace yaxi::refresh;
using namespace yaxi::test;
using json = nlohmann::json;

namespace {
constexpr auto DEMO_CONNECTION_ID = "connection-96386142-60e5-4ca9-abcf-944efce5bc1e";
constexpr auto DEMO_IBAN = "DE02120300000000202051";

ServiceResult &requireServiceResult(Result<ServiceResult> &result) {
    REQUIRE_THAT(result, HoldsAlternative<ServiceResult>());
    return std::get<ServiceResult>(result);
}
} // namespace

TEST_CASE_METHOD(OnlineFixture, "Service Accounts (non-interactive)", "[online][accounts]") {
    auto connectionData = bootstrapConnectionData();
    auto ticket = generator.accounts(uuidV4());

    auto result = client.accounts(connectionData, ticket, {AccountField::Iban, AccountField::Type});
    auto &sr = requireServiceResult(result);

    // The non-interactive response carries the decoded output (an array of
    // accounts) as JSON, not a signed JWT.
    auto accounts = json::parse(sr.json);
    REQUIRE(accounts.is_array());
    REQUIRE_FALSE(accounts.empty());
}

TEST_CASE_METHOD(OnlineFixture, "Service Balances (non-interactive)", "[online][balances]") {
    auto connectionData = bootstrapConnectionData();
    auto ticket = generator.balances(uuidV4());

    std::vector<AccountReference> accounts{
        AccountReference{.iban = DEMO_IBAN, .currency = std::string{"EUR"}},
    };
    auto result = client.balances(connectionData, ticket, accounts);
    auto &sr = requireServiceResult(result);

    auto balances = json::parse(sr.json);
    REQUIRE(balances.contains("balances"));
}

TEST_CASE_METHOD(OnlineFixture, "Service Transactions (non-interactive)",
                 "[online][transactions]") {
    auto connectionData = bootstrapConnectionData();

    std::chrono::year_month_day from{
        std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) -
        std::chrono::days{89}};

    json data = {
        {"account", {{"iban", DEMO_IBAN}, {"currency", "EUR"}}},
        {"range",
         {{"from",
           std::format("{:04}-{:02}-{:02}", static_cast<int>(from.year()),
                       static_cast<unsigned>(from.month()), static_cast<unsigned>(from.day()))}}},
    };
    auto ticket = generator.transactions(uuidV4(), data);

    auto result = client.transactions(connectionData, ticket);
    auto &sr = requireServiceResult(result);

    // The demo connection's transactions predate the SCA-exempt window, so the
    // non-interactive read returns an empty list.
    auto transactions = json::parse(sr.json);
    REQUIRE(transactions == json::array());
}

TEST_CASE_METHOD(OnlineFixture, "connectionData round-trip", "[online][refresh]") {
    auto connectionData = bootstrapConnectionData();

    auto accountsTicket = generator.accounts(uuidV4());
    auto accountsResult = client.accounts(connectionData, accountsTicket, {AccountField::Iban});
    auto &accountsSr = requireServiceResult(accountsResult);

    // A refreshed `connectionData` (when returned) must be usable to drive a
    // subsequent non-interactive call; fall back to the original otherwise.
    auto next = accountsSr.connectionData.value_or(connectionData);

    auto balancesTicket = generator.balances(uuidV4());
    std::vector<AccountReference> accounts{
        AccountReference{.iban = DEMO_IBAN, .currency = std::string{"EUR"}},
    };
    auto balancesResult = client.balances(next, balancesTicket, accounts);
    requireServiceResult(balancesResult);
}

TEST_CASE_METHOD(OnlineFixture, "Connection info", "[online][search]") {
    auto accountsTicket = generator.accounts(uuidV4());

    std::vector<SearchFilter> filters{
        TermSearchFilter{.term = "sparkasse"},
        TermSearchFilter{.term = "stadt"},
    };

    auto result = client.search(accountsTicket, filters, /*ibanDetection=*/true, /*limit=*/20);
    REQUIRE_THAT(result, HoldsAlternative<std::vector<ConnectionInfo>>());
    auto &infos = std::get<std::vector<ConnectionInfo>>(result);

    struct ExpectedInfo {
        std::string id;
        std::string displayName;
    };
    const std::vector<ExpectedInfo> expected{
        {"connection-3c0ec6db-f1e0-4bfb-be0b-4e5af8a7ad4e", "Sparkasse Duderstadt"},
        {"connection-8af40d65-8393-41c4-8c78-09cc652a8f26", "Stadtsparkasse Wedel"},
        {"connection-c5d12d75-0c31-4883-aaaa-f5535e9c82da", "Stadtsparkasse Dessau"},
        {"connection-6227b304-71d9-4cab-b7c1-1bcfcc16fb23", "Stadtsparkasse Rahden"},
        {"connection-099c2821-a46e-4d88-a0c1-5feda0ab63e2", "Stadtsparkasse Rheine"},
        {"connection-499673bd-9907-4c26-88fc-172af8c8032c", "Stadtsparkasse Bocholt"},
        {"connection-3c60b72f-f706-43f8-883c-1dbd1b06ac6c", "Stadtsparkasse München"},
        {"connection-6eb60518-dc4c-4d09-aa42-e38453d5c366", "Stadtsparkasse Schwedt"},
        {"connection-069d6c77-db02-4037-8ccd-7d99b1353c82", "Stadtsparkasse Augsburg"},
        {"connection-8d762a37-df3d-44c3-ba0f-c928a263b360", "Stadtsparkasse Cuxhaven"},
        {"connection-51ecdf0f-6984-4d6c-9420-ec9ba66a01c8", "Stadt-Sparkasse Solingen"},
        {"connection-8eca5600-89aa-487b-b766-a2a8c9bf6e07", "Stadtsparkasse Lengerich"},
        {"connection-41c751be-16dd-4527-9ba8-98ccf65f1520", "Stadtsparkasse Remscheid"},
        {"connection-1d4e32aa-6d1c-4ab1-81ff-7f6d1870af4e", "Stadtsparkasse Wuppertal"},
        {"connection-889582a7-49b7-4b04-8095-ec82d3e820aa", "Stadtsparkasse Düsseldorf"},
        {"connection-931f52d1-b108-4914-a75a-5b3d0715332f", "Stadtsparkasse Oberhausen"},
        {"connection-0ebbaba4-a62b-48fa-b18c-b1f297eb5cc7", "Sparkasse Arnstadt-Ilmenau"},
        {"connection-24570ecb-a2c0-4e2b-9448-1a1eef9c4009", "Stadt-Sparkasse Langenfeld"},
        {"connection-a997832f-a868-45e5-8229-d0fb66af8e46", "Stadtsparkasse Bad Pyrmont"},
        {"connection-7a30cb24-fbc6-44ad-a031-c00a1c248094", "Stadtsparkasse Grebenstein"},
    };

    REQUIRE(infos.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("entry " << i);
        CHECK(infos[i].id == expected[i].id);
        CHECK(infos[i].displayName == expected[i].displayName);
        CHECK(infos[i].countries == std::vector<std::string>{"DE"});
        CHECK(infos[i].credentials.full == true);
        CHECK(infos[i].credentials.userId == false);
        CHECK(infos[i].credentials.none == true);
        CHECK(infos[i].logoId == "sparkasse");
        CHECK(infos[i].userId == std::optional<std::string>{"Anmeldename"});
        CHECK(infos[i].password == std::optional<std::string>{"Online-Banking-PIN"});
    }
}

TEST_CASE_METHOD(OnlineFixture, "Search with BIC and bank code details", "[online][search]") {
    auto accountsTicket = generator.accounts(uuidV4());
    std::vector<SearchFilter> filters{NameSearchFilter{.name = "C24 Bank"}};
    auto result = client.search(accountsTicket, filters, /*ibanDetection=*/false,
                                /*limit=*/std::nullopt, {Details::Bics, Details::BankCodes});
    REQUIRE_THAT(result, HoldsAlternative<std::vector<ConnectionInfo>>());
    auto &infos = std::get<std::vector<ConnectionInfo>>(result);
    REQUIRE_FALSE(infos.empty());
    for (auto &info : infos) {
        REQUIRE(info.bics.has_value());
        CHECK(*info.bics == std::vector<std::string>{"DEFFDEFFXXX"});
        REQUIRE(info.bankCodes.has_value());
        CHECK(*info.bankCodes == std::vector<std::string>{"50024024"});
    }
}

TEST_CASE_METHOD(OnlineFixture, "Error: expired ticket", "[online][errors]") {
    auto expiredAt = std::chrono::system_clock::now() - std::chrono::minutes(5);
    auto ticket = generator.accounts(uuidV4(), expiredAt);

    auto result = client.info(ticket, DEMO_CONNECTION_ID);
    REQUIRE_THAT(result, HoldsAlternative<Error>());
    auto &err = std::get<Error>(result);
    REQUIRE_THAT(err, HoldsAlternative<TicketError>());
    auto &ticketError = std::get<TicketError>(err);
    REQUIRE(std::string{ticketError.what()} == "Ticket is expired");
    REQUIRE(ticketError.code == TicketError::Code::Expired);
}
