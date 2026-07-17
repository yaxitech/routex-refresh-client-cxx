#include "helpers.hpp"
#include "ticket.hpp"

#include <catch2/catch_test_macros.hpp>
#include <yaxi/routex-refresh-client.h>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

using namespace yaxi::refresh;
using namespace yaxi::test;

namespace {
ServiceResult requireServiceResult(Result<ServiceResult> &result) {
    REQUIRE_THAT(result, HoldsAlternative<ServiceResult>());
    return std::get<ServiceResult>(std::move(result));
}

const ConnectionData &sharedConnectionData() {
    static const ConnectionData connectionData = bootstrapConnectionData();
    return connectionData;
}
} // namespace

TEST_CASE_METHOD(OnlineFixture, "AccountFilter", "[online][account_filter]") {
    auto const &connectionData = sharedConnectionData();
    auto fetchAccounts = [&](std::optional<AccountFilter> filter) -> nlohmann::json {
        auto ticket = generator.accounts(uuidV4());
        auto result =
            client.accounts(connectionData, ticket, {AccountField::Iban}, std::move(filter));
        auto sr = requireServiceResult(result);
        return nlohmann::json::parse(sr.json);
    };

    SECTION("all with 0 elements (always true)") {
        auto accounts = fetchAccounts(std::nullopt);
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("all with 1 element") {
        auto accounts = fetchAccounts(AccountField::Iban != std::optional<std::string>{});
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("all with 1 element (no match)") {
        auto accounts = fetchAccounts(AccountField::Iban == std::optional<std::string>{});
        REQUIRE(accounts.empty());
    }

    SECTION("all with 2 elements") {
        auto accounts = fetchAccounts((AccountField::Iban != std::optional<std::string>{}) &&
                                      AccountFilter::supports(SupportedService::CollectPayment));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("all with 2 elements (no match)") {
        auto accounts = fetchAccounts((AccountField::Iban != std::optional<std::string>{}) &&
                                      (AccountField::Iban == std::optional<std::string>{}));
        REQUIRE(accounts.empty());
    }

    SECTION("all with more than 2 elements") {
        auto accounts = fetchAccounts((AccountField::Iban != std::optional<std::string>{}) &&
                                      AccountFilter::supports(SupportedService::CollectPayment) &&
                                      (AccountField::Bic != std::optional<std::string>{}));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("all with more than 2 elements (no match)") {
        auto accounts = fetchAccounts((AccountField::Iban != std::optional<std::string>{}) &&
                                      AccountFilter::supports(SupportedService::CollectPayment) &&
                                      (AccountField::Iban == std::optional<std::string>{}));
        REQUIRE(accounts.empty());
    }

    SECTION("any with 1 element") {
        auto accounts = fetchAccounts(AccountFilter::supports(SupportedService::CollectPayment));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("any with 1 element (no match)") {
        auto accounts = fetchAccounts(AccountField::Iban == std::optional<std::string>{});
        REQUIRE(accounts.empty());
    }

    SECTION("any with 2 elements") {
        auto accounts = fetchAccounts((AccountField::Iban == std::optional<std::string>{}) ||
                                      (AccountField::Iban != std::optional<std::string>{}));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("any with 2 elements (no match)") {
        auto accounts = fetchAccounts((AccountField::Iban == std::optional<std::string>{}) ||
                                      (AccountField::Bic == std::optional<std::string>{}));
        REQUIRE(accounts.empty());
    }

    SECTION("any with more than 2 elements") {
        auto accounts = fetchAccounts((AccountField::Iban == std::optional<std::string>{}) ||
                                      (AccountField::Bic == std::optional<std::string>{}) ||
                                      (AccountField::Iban != std::optional<std::string>{}));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("any with more than 2 elements (no match)") {
        auto accounts = fetchAccounts((AccountField::Iban == std::optional<std::string>{}) ||
                                      (AccountField::Bic == std::optional<std::string>{}) ||
                                      (AccountField::Iban == std::optional<std::string>{}));
        REQUIRE(accounts.empty());
    }

    SECTION("nested any inside all") {
        auto accounts = fetchAccounts((AccountField::Iban != std::optional<std::string>{}) &&
                                      ((AccountField::Iban == std::optional<std::string>{}) ||
                                       AccountFilter::supports(SupportedService::CollectPayment)));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("nested any inside all (no match)") {
        auto accounts = fetchAccounts((AccountField::Iban != std::optional<std::string>{}) &&
                                      ((AccountField::Iban == std::optional<std::string>{}) ||
                                       (AccountField::Bic == std::optional<std::string>{})));
        REQUIRE(accounts.empty());
    }

    SECTION("nested all inside any") {
        auto accounts = fetchAccounts((AccountField::Iban == std::optional<std::string>{}) ||
                                      ((AccountField::Iban != std::optional<std::string>{}) &&
                                       AccountFilter::supports(SupportedService::CollectPayment)));
        REQUIRE_FALSE(accounts.empty());
    }

    SECTION("nested all inside any (no match)") {
        auto accounts = fetchAccounts((AccountField::Iban == std::optional<std::string>{}) ||
                                      ((AccountField::Iban != std::optional<std::string>{}) &&
                                       (AccountField::Iban == std::optional<std::string>{})));
        REQUIRE(accounts.empty());
    }
}
