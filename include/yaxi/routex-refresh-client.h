#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#if defined(_WIN32)
#if defined(YAXI_BUILDING_DLL)
#define YAXI_API __declspec(dllexport)
#else
#define YAXI_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define YAXI_API __attribute__((visibility("default")))
#else
#define YAXI_API
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251) // member needs dll-interface
#pragma warning(disable : 4275) // non dll-interface base class (std::runtime_error)
#endif

namespace yaxi::refresh {
using ConnectionId = std::string;
using ConnectionData = std::vector<uint8_t>;
using Session = std::vector<uint8_t>;

class YAXI_API RequestError : public std::runtime_error {
  public:
    RequestError(std::string error)
        : std::runtime_error("Request error"), error(std::move(error)) {}

    std::string error;
};

class YAXI_API UnexpectedError : public std::runtime_error {
  public:
    UnexpectedError(std::optional<std::string> userMessage)
        : std::runtime_error("Unexpected service error"), userMessage(std::move(userMessage)) {}

    std::optional<std::string> userMessage;
};

class YAXI_API Canceled : public std::runtime_error {
  public:
    Canceled() : std::runtime_error("Canceled") {}
};

class YAXI_API InvalidCredentials : public std::runtime_error {
  public:
    InvalidCredentials(std::optional<std::string> userMessage)
        : std::runtime_error("Invalid credentials"), userMessage(std::move(userMessage)) {}

    std::optional<std::string> userMessage;
};

class YAXI_API ServiceBlocked : public std::runtime_error {
  public:
    enum class Code : uint8_t {
        /**
         * Something is not set up for the user, e.g., there are no TAN methods.
         */
        MissingSetup,
        /**
         * User attention is required via another channel.
         * Typically the user needs to log into the Online Banking.
         */
        ActionRequired,
    };

    ServiceBlocked(std::optional<std::string> userMessage, std::optional<Code> code)
        : std::runtime_error("Service blocked"), userMessage(std::move(userMessage)), code(code) {}

    std::optional<std::string> userMessage;
    std::optional<Code> code;
};

class YAXI_API Unauthorized : public std::runtime_error {
  public:
    Unauthorized(std::optional<std::string> userMessage)
        : std::runtime_error("Unauthorized"), userMessage(std::move(userMessage)) {}

    std::optional<std::string> userMessage;
};

class YAXI_API AccessExceeded : public std::runtime_error {
  public:
    AccessExceeded(std::optional<std::string> userMessage)
        : std::runtime_error("Access exceeded"), userMessage(std::move(userMessage)) {}

    std::optional<std::string> userMessage;
};

class YAXI_API PeriodOutOfBounds : public std::runtime_error {
  public:
    PeriodOutOfBounds(std::optional<std::string> userMessage)
        : std::runtime_error("Period out of bounds"), userMessage(std::move(userMessage)) {}

    std::optional<std::string> userMessage;
};

class YAXI_API UnsupportedProduct : public std::runtime_error {
  public:
    enum class Reason : uint8_t {
        /**
         * The amount is not allowed for the payment product.
         */
        Limit,

        /**
         * The recipient is not capable to receive the payment product.
         */
        Recipient,
    };

    UnsupportedProduct(std::optional<std::string> userMessage, std::optional<Reason> reason)
        : std::runtime_error("Unsupported product"), userMessage(std::move(userMessage)),
          reason(reason) {}

    std::optional<std::string> userMessage;
    std::optional<Reason> reason;
};

class YAXI_API PaymentFailed : public std::runtime_error {
  public:
    enum class Code : uint8_t {
        LimitExceeded,
        InsufficientFunds,
    };

    PaymentFailed(std::optional<std::string> userMessage, std::optional<Code> code)
        : std::runtime_error("Payment canceled or rejected"), userMessage(std::move(userMessage)),
          code(code) {}

    std::optional<std::string> userMessage;
    std::optional<Code> code;
};

class YAXI_API UnexpectedValue : public std::runtime_error {
  public:
    UnexpectedValue(std::string error)
        : std::runtime_error("Unexpected value"), error(std::move(error)) {}

    std::string error;
};

class YAXI_API TicketError : public std::runtime_error {
  public:
    enum class Code : uint8_t {
        Missing,
        Invalid,
        MissingKey,
        UnknownKey,
        Mismatch,
        Expired,
        InvalidLifetime,
        ExpiredKey,
        KeyEnvironmentMismatch,
    };

    TicketError(std::string error, Code code)
        : std::runtime_error(error), error(std::move(error)), code(code) {}

    std::string error;
    Code code;
};

class YAXI_API ProviderError : public std::runtime_error {
  public:
    enum class Code : uint8_t {
        Maintenance,
    };

    ProviderError(std::optional<std::string> userMessage, std::optional<Code> code)
        : std::runtime_error("The account-servicing provider indicated an error"),
          userMessage(std::move(userMessage)), code(code) {}

    std::optional<std::string> userMessage;
    std::optional<Code> code;
};

class YAXI_API ResponseError : public std::runtime_error {
  public:
    ResponseError(std::string response)
        : std::runtime_error("Error response"), response(std::move(response)) {}

    std::string response;
};

class YAXI_API NotFound : public std::runtime_error {
  public:
    NotFound() : std::runtime_error("Resource not found") {}
};

using Error =
    std::variant<RequestError, UnexpectedError, Canceled, InvalidCredentials, ServiceBlocked,
                 Unauthorized, AccessExceeded, PeriodOutOfBounds, UnsupportedProduct, PaymentFailed,
                 UnexpectedValue, TicketError, ProviderError, ResponseError, NotFound>;

template <typename T> using Result = std::variant<T, Error>;

struct ServiceResult {
    std::string json;
    std::optional<Session> session;
    std::optional<ConnectionData> connectionData;
};

/**
 * Requirements for user identifier and password.
 */
struct CredentialsModel {
    /**
     * A full set of credentials may be provided to support fully embedded authentication (including
     * scraped redirects).
     */
    bool full;

    /**
     * Only a user identifier without a password may be provided.
     * This is typically the case for decoupled authentication where the user e.g. authorizes access
     * in a mobile application. Note that if password-less authentication fails (e.g. as no device
     * for decoupled authentication is set up for the user and a redirect is not supported), an
     * error is returned and the transaction has to get restarted with a full set of credentials.
     */
    bool userId;

    /**
     * Credentials are not required. The user will provide them to the service provider during a
     * redirect.
     */
    bool none;
};

/**
 * Connection meta data
 */
struct ConnectionInfo {
    /**
     * Unique identifier.
     */
    ConnectionId id;

    /**
     * ISO 3166-1 ALPHA-2 country codes.
     */
    std::vector<std::string> countries;

    /**
     * Display name.
     */
    std::string displayName;

    /**
     * Credentials model.
     */
    CredentialsModel credentials;

    /**
     * Human-friendly label for the user identifier if relevant.
     */
    std::optional<std::string> userId;

    /**
     * Human-friendly label for the PIN / password if relevant.
     */
    std::optional<std::string> password;

    /**
     * Advice for the credentials to be displayed.
     */
    std::optional<std::string> advice;

    /**
     * Logo identifier.
     */
    std::string logoId;

    /**
     * ISO 20022 BICFIIdentifiers.
     *
     * Note that this is only included in search results if requested.
     */
    std::optional<std::vector<std::string>> bics;

    /**
     * National bank codes (as used in IBANs).
     *
     * Note that this is only included in search results if requested.
     */
    std::optional<std::vector<std::string>> bankCodes;
};

/**
 * List of ISO 3166-1 alpha-2 country codes to consider.
 */
struct CountriesSearchFilter {
    std::vector<std::string> countries;
};

/**
 * String filter for the provider / product name or any alias.
 */
struct NameSearchFilter {
    std::string name;
};

/**
 * String filter for the BIC.
 */
struct BicSearchFilter {
    std::string bic;
};

/**
 * String filter for the (national) bank code.
 */
struct BankCodeSearchFilter {
    std::string bankCode;
};

/**
 * String filter for any of those fields.
 */
struct TermSearchFilter {
    std::string term;
};

/**
 * Filters for the connection lookup
 *
 * String filters look for the given value anywhere in the related field, case-insensitive.
 */
using SearchFilter = std::variant<CountriesSearchFilter, NameSearchFilter, BicSearchFilter,
                                  BankCodeSearchFilter, TermSearchFilter>;

/**
 * Details to contain in search results.
 */
enum class Details : uint8_t {
    Bics,
    BankCodes,
};

enum class AccountStatus : uint8_t {
    Available,
    Terminated,
    Blocked,
};

enum class AccountType : uint8_t {
    /**
     * Account used to post debits and credits.
     * ISO 20022 ExternalCashAccountType1Code CACC.
     */
    Current,
    /**
     * Account used for credit card payments.
     * ISO 20022 ExternalCashAccountType1Code CARD.
     */
    Card,
    /**
     * Account used for savings.
     * ISO 20022 ExternalCashAccountType1Code SVGS.
     */
    Savings,
    /**
     * Account used for call money.
     * No dedicated ISO 20022 code (falls into SVGS).
     */
    CallMoney,
    /**
     * Account used for time deposits.
     * No dedicated ISO 20022 code (falls into SVGS).
     */
    TimeDeposit,
    /**
     * Account used for loans.
     * ISO 20022 ExternalCashAccountType1Code LOAN.
     */
    Loan,
    Securities,
    Insurance,
    Commerce,
    Rewards,
};

enum class SupportedService : uint8_t {
    CollectPayment,
};

class YAXI_API AccountFilter {
  public:
    struct Inner;
    std::unique_ptr<Inner> inner;

    explicit AccountFilter(Inner inner);
    ~AccountFilter();

    AccountFilter(const AccountFilter &) = delete;
    AccountFilter &operator=(const AccountFilter &) = delete;

    AccountFilter(AccountFilter &&) noexcept;
    AccountFilter &operator=(AccountFilter &&) noexcept;

    AccountFilter operator&&(AccountFilter &&filter) &&;
    AccountFilter operator||(AccountFilter &&filter) &&;

    static AccountFilter supports(const SupportedService &service);
};

class YAXI_API AccountField {
  private:
    uint8_t value;
    AccountField(const uint8_t &value) : value(value) {}
    class OptionalStringField;
    class CurrencyField;
    class StatusField;
    class TypeField;

  public:
    static const OptionalStringField Iban;
    static const OptionalStringField Number;
    static const OptionalStringField Bic;
    static const OptionalStringField BankCode;
    static const CurrencyField Currency;
    static const OptionalStringField Name;
    static const OptionalStringField DisplayName;
    static const OptionalStringField OwnerName;
    static const OptionalStringField ProductName;
    static const StatusField Status;
    static const TypeField Type;

    constexpr bool operator==(const AccountField &other) const { return value == other.value; }
    constexpr operator uint8_t() const { return value; }
};

class YAXI_API AccountField::OptionalStringField : public AccountField {
  public:
    AccountFilter operator==(const std::optional<std::string> &val) const;
    AccountFilter operator!=(const std::optional<std::string> &val) const;
};

class YAXI_API AccountField::CurrencyField : public AccountField {
  public:
    AccountFilter operator==(const std::string &val) const;
    AccountFilter operator!=(const std::string &val) const;
};

class YAXI_API AccountField::StatusField : public AccountField {
  public:
    AccountFilter operator==(const std::optional<AccountStatus> &val) const;
    AccountFilter operator!=(const std::optional<AccountStatus> &val) const;
};

class YAXI_API AccountField::TypeField : public AccountField {
  public:
    AccountFilter operator==(const std::optional<AccountType> &val) const;
    AccountFilter operator!=(const std::optional<AccountType> &val) const;
};

struct AccountReference {
    std::string iban;
    std::optional<std::string> currency;
};

/**
 * Indicates that a user is in session for a non-interactive service call.
 *
 * When a user is in session, YAXI forwards the user's IP address to their bank. That
 * address is either this connection's own source IP (`onThisConnection()`) or an IP
 * address the caller provides (`at()`).
 *
 * Without a user in session, banks limit how many requests a caller may make and reject
 * excess requests with an `AccessExceeded` error. A user in session lifts that limit.
 */
class UserInSession {
  public:
    /** The user's IP is this connection's own source IP. */
    static UserInSession onThisConnection() { return UserInSession(std::nullopt); }

    /** The user is at the given IP address. */
    static UserInSession at(std::string ipAddress) { return UserInSession(std::move(ipAddress)); }

    /** The forwarded address: empty for `onThisConnection()`, set for `at()`. */
    const std::optional<std::string> &ipAddress() const { return ipAddress_; }

  private:
    explicit UserInSession(std::optional<std::string> ipAddress)
        : ipAddress_(std::move(ipAddress)) {}

    std::optional<std::string> ipAddress_;
};

class [[nodiscard]] YAXI_API RoutexRefreshClient final {
  public:
    /**
     * Create a new client, optionally providing a custom URL.
     *
     * `userInSession` indicates a user is in session for every service call this client
     * makes, lifting the bank's limit on requests without one (see `UserInSession`). Omit
     * it (the default) to make calls without a user in session.
     */
    explicit RoutexRefreshClient(const std::optional<std::string> &url = std::nullopt,
                                 const std::optional<UserInSession> &userInSession = std::nullopt);

    ~RoutexRefreshClient();

    RoutexRefreshClient(const RoutexRefreshClient &) = delete;
    RoutexRefreshClient &operator=(const RoutexRefreshClient &) = delete;

    RoutexRefreshClient(RoutexRefreshClient &&) noexcept;
    RoutexRefreshClient &operator=(RoutexRefreshClient &&) noexcept;

    [[nodiscard]] std::optional<std::string> systemVersion(const std::string &ticketId) const;

    /**
     * Search for service connections (banks and other providers).
     *
     * The result is a list of connections that match all the `SearchFilter`s.
     * If IBAN detection is enabled and the first value of a term filter is detected
     * to be a possible prefix of an IBAN that contains a national bank code,
     * the result might contain additional connections that match that bank code.
     */
    [[nodiscard]] Result<std::vector<ConnectionInfo>>
    search(const std::string &ticket, const std::vector<SearchFilter> &filters,
           bool ibanDetection = false, const std::optional<size_t> &limit = std::nullopt,
           const std::vector<Details> &details = std::vector<Details>()) const;

    /**
     * Get information for a service connection.
     */
    [[nodiscard]] Result<ConnectionInfo> info(const std::string &ticket,
                                              ConnectionId connectionId) const;

    /**
     * [Accounts service](https://docs.yaxi.tech/accounts.html).
     */
    [[nodiscard]] Result<ServiceResult>
    accounts(const ConnectionData &connectionData, const std::string &ticket,
             const std::vector<AccountField> &fields,
             const std::optional<AccountFilter> &filter = std::nullopt,
             const std::optional<Session> &session = std::nullopt) const;

    /**
     * [Balances service](https://docs.yaxi.tech/balances.html).
     */
    [[nodiscard]] Result<ServiceResult>
    balances(const ConnectionData &connectionData, const std::string &ticket,
             const std::vector<AccountReference> &accounts,
             const std::optional<Session> &session = std::nullopt) const;

    /**
     * [Transactions service](https://docs.yaxi.tech/transactions.html).
     */
    [[nodiscard]] Result<ServiceResult>
    transactions(const ConnectionData &connectionData, const std::string &ticket,
                 const std::optional<Session> &session = std::nullopt) const;

  private:
    struct Inner;
    std::unique_ptr<Inner> inner;
};
} // namespace yaxi::refresh

#ifdef _MSC_VER
#pragma warning(pop)
#endif
