use anyhow::{anyhow, bail};
use cxx::{CxxString, CxxVector, UniquePtr};
use paste::paste;
use routex_client_common::with_any_service;
use routex_refresh_client::prelude::*;
use tokio::runtime::Runtime;

#[cxx::bridge(namespace = "yaxi::refresh::internal")]
mod ffi {
    extern "Rust" {
        type RoutexRefreshClient;

        fn new_routex_refresh_client(
            url: &UniquePtr<CxxString>,
            user_in_session: &UniquePtr<UserInSession>,
        ) -> Result<Box<RoutexRefreshClient>>;

        fn system_version(self: &RoutexRefreshClient, ticket_id: &CxxString) -> Result<String>;

        fn search(
            self: &RoutexRefreshClient,
            ticket: &CxxString,
            filters: &CxxVector<SearchFilter>,
            iban_detection: bool,
            limit: &UniquePtr<Usize>,
            details: &CxxVector<Details>,
        ) -> Result<SearchResult>;

        fn info(
            self: &RoutexRefreshClient,
            ticket: &CxxString,
            connection_id: &CxxString,
        ) -> Result<InfoResult>;

        fn accounts(
            self: &RoutexRefreshClient,
            connection_data: &CxxVector<u8>,
            ticket: &CxxString,
            fields: &CxxVector<AccountField>,
            filter: &UniquePtr<AccountFilter>,
            session: &UniquePtr<CxxVector<u8>>,
        ) -> Result<ServiceResult>;

        fn balances(
            self: &RoutexRefreshClient,
            connection_data: &CxxVector<u8>,
            ticket: &CxxString,
            accounts: &CxxVector<AccountReference>,
            session: &UniquePtr<CxxVector<u8>>,
        ) -> Result<ServiceResult>;

        fn transactions(
            self: &RoutexRefreshClient,
            connection_data: &CxxVector<u8>,
            ticket: &CxxString,
            session: &UniquePtr<CxxVector<u8>>,
        ) -> Result<ServiceResult>;
    }

    #[derive(Debug)]
    struct SearchResult {
        value: Vec<ConnectionInfo>,
        error: UniquePtr<Error>,
    }

    #[derive(Debug)]
    struct InfoResult {
        value: UniquePtr<ConnectionInfo>,
        error: UniquePtr<Error>,
    }

    #[derive(Debug)]
    struct ServiceResult {
        result: UniquePtr<NonInteractiveResult>,
        error: UniquePtr<Error>,
    }

    #[derive(Debug)]
    struct NonInteractiveResult {
        json: String,
        session: Vec<u8>,
        connection_data: Vec<u8>,
    }

    #[derive(Debug)]
    enum ErrorKind {
        RequestError,
        UnexpectedError,
        Canceled,
        InvalidCredentials,
        ServiceBlocked,
        Unauthorized,
        AccessExceeded,
        PeriodOutOfBounds,
        UnsupportedProduct,
        PaymentFailed,
        UnexpectedValue,
        TicketError,
        ProviderError,
        ResponseError,
        NotFound,
    }

    #[derive(Debug)]
    struct Error {
        kind: ErrorKind,
        string: String,
        code: UniquePtr<U8>,
    }

    #[derive(Debug)]
    struct Usize {
        val: usize,
    }

    #[derive(Debug)]
    struct U8 {
        val: u8,
    }

    #[derive(Debug)]
    struct UserInSession {
        ip_address: UniquePtr<CxxString>,
    }

    #[derive(Debug)]
    struct SearchFilter {
        countries: UniquePtr<CxxVector<CxxString>>,
        name: UniquePtr<CxxString>,
        bic: UniquePtr<CxxString>,
        bank_code: UniquePtr<CxxString>,
        term: UniquePtr<CxxString>,
    }

    #[derive(Debug)]
    enum Details {
        Bics,
        BankCodes,
    }

    #[derive(Debug)]
    struct ConnectionInfo {
        id: String,
        countries: Vec<String>,
        display_name: String,
        credentials_full: bool,
        credentials_user: bool,
        credentials_none: bool,
        user_id: String,
        password: String,
        advice: String,
        logo_id: String,
        bics_set: bool,
        bics: Vec<String>,
        bank_codes_set: bool,
        bank_codes: Vec<String>,
    }

    #[derive(Debug)]
    enum AccountField {
        Iban,
        Number,
        Bic,
        BankCode,
        Currency,
        Name,
        DisplayName,
        OwnerName,
        ProductName,
        Status,
        Type,
    }

    #[derive(Debug)]
    struct AccountFilter {
        eq_field: UniquePtr<AccountField>,
        neq_field: UniquePtr<AccountField>,
        string: UniquePtr<CxxString>,
        status: UniquePtr<AccountStatus>,
        type_: UniquePtr<AccountType>,
        lhs: UniquePtr<AccountFilter>,
        and_rhs: UniquePtr<AccountFilter>,
        or_rhs: UniquePtr<AccountFilter>,
        supports: UniquePtr<SupportedService>,
    }

    #[derive(Debug)]
    enum AccountStatus {
        Available,
        Terminated,
        Blocked,
    }

    #[derive(Debug)]
    enum AccountType {
        Current,
        Card,
        Savings,
        CallMoney,
        TimeDeposit,
        Loan,
        Securities,
        Insurance,
        Commerce,
        Rewards,
    }

    #[derive(Debug)]
    enum SupportedService {
        CollectPayment,
    }

    #[derive(Debug)]
    struct AccountReference {
        iban: String,
        currency: UniquePtr<CxxString>,
    }

    #[namespace = "yaxi::refresh"]
    unsafe extern "C++" {
        include!("yaxi/routex-refresh-client.h");
        type AccountStatus;
        type AccountType;
        type Details;
        type SupportedService;
    }
}

impl From<routex_client_common::Error> for ffi::Error {
    fn from(err: routex_client_common::Error) -> Self {
        use ffi::ErrorKind;
        use routex_client_common::Error;

        let (kind, string, code) = match err {
            Error::RequestError(error) => {
                (ErrorKind::RequestError, Some(format!("{error:?}")), None)
            }
            Error::ServiceError(error) => match error {
                routex_api::Error::UnexpectedError { user_message, .. } => {
                    (ErrorKind::UnexpectedError, user_message, None)
                }
                routex_api::Error::Canceled { .. } => (ErrorKind::Canceled, None, None),
                routex_api::Error::InvalidCredentials { user_message, .. } => {
                    (ErrorKind::InvalidCredentials, user_message, None)
                }
                routex_api::Error::ServiceBlocked {
                    user_message, code, ..
                } => (
                    ErrorKind::ServiceBlocked,
                    user_message,
                    code.map(|c| c as u8),
                ),
                routex_api::Error::Unauthorized { user_message, .. } => {
                    (ErrorKind::Unauthorized, user_message, None)
                }
                routex_api::Error::AccessExceeded { user_message, .. } => {
                    (ErrorKind::AccessExceeded, user_message, None)
                }
                routex_api::Error::PeriodOutOfBounds { user_message, .. } => {
                    (ErrorKind::PeriodOutOfBounds, user_message, None)
                }
                routex_api::Error::UnsupportedProduct {
                    reason,
                    user_message,
                    ..
                } => (
                    ErrorKind::UnsupportedProduct,
                    user_message,
                    reason.map(|c| c as u8),
                ),
                routex_api::Error::PaymentFailed {
                    code, user_message, ..
                } => (
                    ErrorKind::PaymentFailed,
                    user_message,
                    code.map(|c| c as u8),
                ),
                routex_api::Error::UnexpectedValue { error, .. } => {
                    (ErrorKind::UnexpectedValue, Some(error), None)
                }
                routex_api::Error::TicketError { error, code, .. } => {
                    (ErrorKind::TicketError, Some(error), Some(code as u8))
                }
                routex_api::Error::ProviderError {
                    code, user_message, ..
                } => (
                    ErrorKind::ProviderError,
                    user_message,
                    code.map(|c| c as u8),
                ),
                routex_api::Error::InterruptError { .. } => {
                    (ErrorKind::ResponseError, Some(String::new()), None)
                }
            },
            Error::ResponseError(response) => (
                ErrorKind::ResponseError,
                Some(format!("{response:?}")),
                None,
            ),
            Error::NotFound => (ErrorKind::NotFound, None, None),
        };

        ffi::Error {
            kind,
            string: string.unwrap_or_default(),
            code: code.map_or_else(UniquePtr::null, |val| UniquePtr::new(ffi::U8 { val })),
        }
    }
}

impl From<jsonwebtoken::errors::Error> for ffi::Error {
    fn from(err: jsonwebtoken::errors::Error) -> Self {
        Self {
            kind: ffi::ErrorKind::TicketError,
            string: err.to_string(),
            code: UniquePtr::new(ffi::U8 {
                val: TicketErrorCode::Invalid as u8,
            }),
        }
    }
}

impl Default for ffi::ServiceResult {
    fn default() -> Self {
        Self {
            result: UniquePtr::null(),
            error: UniquePtr::null(),
        }
    }
}

impl<S, E> From<Result<NonInteractiveResponse<S>, E>> for ffi::ServiceResult
where
    S: routex_api::Service,
    ffi::Error: From<E>,
{
    fn from(value: Result<NonInteractiveResponse<S>, E>) -> Self {
        match value {
            Ok(response) => match serde_json::to_string(&response.result) {
                Ok(json) => Self {
                    result: UniquePtr::new(ffi::NonInteractiveResult {
                        json,
                        session: response.session.map_or_else(Vec::new, Into::into),
                        connection_data: response.connection_data.map_or_else(Vec::new, Into::into),
                    }),
                    error: UniquePtr::null(),
                },
                Err(err) => routex_client_common::Error::RequestError(err.into()).into(),
            },
            Err(err) => err.into(),
        }
    }
}

impl<E> From<E> for ffi::ServiceResult
where
    ffi::Error: From<E>,
{
    fn from(err: E) -> Self {
        Self {
            error: UniquePtr::new(err.into()),
            ..Default::default()
        }
    }
}

impl From<ConnectionInfo> for ffi::ConnectionInfo {
    fn from(info: ConnectionInfo) -> Self {
        Self {
            id: info.id.to_string(),
            countries: info
                .countries
                .into_iter()
                .map(|code| code.alpha2().to_string())
                .collect(),
            display_name: info.display_name,
            credentials_full: info.credentials.full,
            credentials_user: info.credentials.user_id,
            credentials_none: info.credentials.none,
            user_id: info.user_id.unwrap_or_default(),
            password: info.password.unwrap_or_default(),
            advice: info.advice.unwrap_or_default(),
            logo_id: info.logo_id,
            bics_set: info.bics.is_some(),
            bics: info.bics.unwrap_or_default(),
            bank_codes_set: info.bank_codes.is_some(),
            bank_codes: info.bank_codes.unwrap_or_default(),
        }
    }
}

macro_rules! account_fields {
    {
        $filter:ident
        $($field:ident $get_val:tt)+
    } => {
        impl TryFrom<ffi::AccountField> for AccountField {
            type Error = anyhow::Error;

            fn try_from(field: ffi::AccountField) -> Result<Self, Self::Error> {
                Ok(match field {
                    $(ffi::AccountField::$field => AccountField::$field,)+
                    _ => bail!("Unexpected AccountField value"),
                })
            }
        }

        paste! {
            impl TryFrom<&ffi::AccountFilter> for Filter<AccountField> {
                type Error = anyhow::Error;

                #[allow(unused_braces)]
                fn try_from($filter: &ffi::AccountFilter) -> Result<Self, Self::Error> {
                    Ok(if let Some(eq_field) = $filter.eq_field.as_ref() {
                        match *eq_field {
                            $(
                                ffi::AccountField::$field => AccountField::[<$field:snake:upper>]
                                    .eq($get_val),
                            )+
                            _ => bail!("Unexpected AccountField value"),
                        }
                    } else if let Some(neq_field) = $filter.neq_field.as_ref() {
                        match *neq_field {
                            $(
                                ffi::AccountField::$field => AccountField::[<$field:snake:upper>]
                                    .not_eq($get_val),
                            )+
                            _ => bail!("Unexpected AccountField value"),
                        }
                    } else if let Some(and_rhs) = $filter.and_rhs.as_ref() {
                        Filter::try_from($filter.lhs.as_ref().ok_or(anyhow!("Invalid filter"))?)?
                            .and(and_rhs.try_into()?)
                    } else if let Some(or_rhs) = $filter.or_rhs.as_ref() {
                        Filter::try_from($filter.lhs.as_ref().ok_or(anyhow!("Invalid filter"))?)?
                            .or(or_rhs.try_into()?)
                    } else {
                        Account::supports(
                            match *$filter.supports.as_ref().ok_or(anyhow!("Invalid filter"))? {
                                ffi::SupportedService::CollectPayment => SupportedService::CollectPayment,
                                _ => bail!("Unexpected SupportedService value"),
                            }
                        )
                    })
                }
            }
        }
    }
}

account_fields! {
    filter
    Iban { filter.string.as_ref().map(ToString::to_string) }
    Number { filter.string.as_ref().map(ToString::to_string) }
    Bic { filter.string.as_ref().map(ToString::to_string) }
    BankCode { filter.string.as_ref().map(ToString::to_string) }
    Currency { filter.string.as_ref().ok_or(anyhow!("Invalid filter value"))?.to_string() }
    Name { filter.string.as_ref().map(ToString::to_string) }
    DisplayName { filter.string.as_ref().map(ToString::to_string) }
    OwnerName { filter.string.as_ref().map(ToString::to_string) }
    ProductName { filter.string.as_ref().map(ToString::to_string) }
    Status { filter.status.as_ref().and_then(|status| Some(match *status {
        ffi::AccountStatus::Available => AccountStatus::Available,
        ffi::AccountStatus::Terminated => AccountStatus::Terminated,
        ffi::AccountStatus::Blocked => AccountStatus::Blocked,
        _ => return None,
    })) }
    Type { filter.type_.as_ref().and_then(|type_| Some(match *type_ {
        ffi::AccountType::Current => AccountType::Current,
        ffi::AccountType::Card => AccountType::Card,
        ffi::AccountType::Savings => AccountType::Savings,
        ffi::AccountType::CallMoney => AccountType::CallMoney,
        ffi::AccountType::TimeDeposit => AccountType::TimeDeposit,
        ffi::AccountType::Loan => AccountType::Loan,
        ffi::AccountType::Securities => AccountType::Securities,
        ffi::AccountType::Insurance => AccountType::Insurance,
        ffi::AccountType::Commerce => AccountType::Commerce,
        ffi::AccountType::Rewards => AccountType::Rewards,
        _ => return None,
    })) }
}

impl From<&ffi::AccountReference> for AccountReference {
    fn from(account: &ffi::AccountReference) -> Self {
        AccountReference {
            id: AccountIdentifier::Iban(account.iban.clone()),
            currency: account.currency.as_ref().map(CxxString::to_string),
        }
    }
}

struct RoutexRefreshClient {
    inner: routex_refresh_client::RoutexRefreshClient<reqwest::Client>,
    runtime: Runtime,
}

fn new_routex_refresh_client(
    url: &UniquePtr<CxxString>,
    user_in_session: &UniquePtr<ffi::UserInSession>,
) -> anyhow::Result<Box<RoutexRefreshClient>> {
    let mut inner = routex_refresh_client::RoutexRefreshClient::for_distribution(
        "C++",
        env!("CARGO_PKG_VERSION"),
        url.as_ref()
            .map_or("https://api.yaxi.tech/".parse(), |url| {
                url.to_string().parse()
            })?,
        reqwest::Client::new(),
    );

    if let Some(user_in_session) = user_in_session.as_ref() {
        inner = inner.user_in_session(match user_in_session.ip_address.as_ref() {
            Some(ip_address) => UserInSession::At(ip_address.to_string().parse()?),
            None => UserInSession::OnThisConnection,
        });
    }

    Ok(Box::new(RoutexRefreshClient {
        inner,
        runtime: Runtime::new()?,
    }))
}

impl RoutexRefreshClient {
    fn system_version(&self, ticket_id: &CxxString) -> anyhow::Result<String> {
        Ok(self
            .runtime
            .block_on(self.inner.system_version(ticket_id.to_string().parse()?))
            .unwrap_or_default())
    }

    fn search(
        &self,
        ticket: &CxxString,
        filters: &CxxVector<ffi::SearchFilter>,
        iban_detection: bool,
        limit: &UniquePtr<ffi::Usize>,
        details: &CxxVector<ffi::Details>,
    ) -> anyhow::Result<ffi::SearchResult> {
        let ticket = ticket.to_string();

        let filters = filters
            .into_iter()
            .map(|f| {
                Ok(if let Some(countries) = f.countries.as_ref() {
                    SearchFilter::Countries(
                        countries
                            .iter()
                            .flat_map(|c| CountryCode::for_alpha2(&c.to_string()))
                            .collect(),
                    )
                } else if let Some(name) = f.name.as_ref() {
                    SearchFilter::Name(name.to_string())
                } else if let Some(bic) = f.bic.as_ref() {
                    SearchFilter::Bic(bic.to_string())
                } else if let Some(bank_code) = f.bank_code.as_ref() {
                    SearchFilter::BankCode(bank_code.to_string())
                } else {
                    SearchFilter::Term(
                        f.term
                            .as_ref()
                            .ok_or(anyhow!("Invalid filter"))?
                            .to_string(),
                    )
                })
            })
            .collect::<anyhow::Result<Vec<_>>>()?;

        Ok(
            match with_any_service!(ticket, authenticated, {
                self.runtime.block_on(
                    self.inner
                        .search(authenticated, filters)
                        .iban_detection(iban_detection)
                        .limit(limit.as_ref().map(|limit| limit.val))
                        .details(
                            details
                                .into_iter()
                                .map(|detail| {
                                    Ok(match *detail {
                                        ffi::Details::Bics => Details::Bics,
                                        ffi::Details::BankCodes => Details::BankCodes,
                                        _ => bail!("Unexpected Details value"),
                                    })
                                })
                                .collect::<Result<Vec<_>, _>>()?,
                        )
                        .send(),
                )
            }) as Result<Vec<_>, ffi::Error>
            {
                Ok(connections) => ffi::SearchResult {
                    value: connections.into_iter().map(Into::into).collect(),
                    error: UniquePtr::null(),
                },
                Err(err) => ffi::SearchResult {
                    value: Vec::new(),
                    error: UniquePtr::new(err),
                },
            },
        )
    }

    fn info(
        &self,
        ticket: &CxxString,
        connection_id: &CxxString,
    ) -> Result<ffi::InfoResult, uuid::Error> {
        let ticket = ticket.to_string();

        match with_any_service!(ticket, authenticated, {
            self.runtime.block_on(
                self.inner
                    .info(&authenticated, connection_id.to_string().parse()?),
            )
        }) as Result<ConnectionInfo, ffi::Error>
        {
            Ok(info) => Ok(ffi::InfoResult {
                value: UniquePtr::new(info.into()),
                error: UniquePtr::null(),
            }),
            Err(err) => Ok(ffi::InfoResult {
                value: UniquePtr::null(),
                error: UniquePtr::new(err),
            }),
        }
    }
}

macro_rules! service {
    {
        $service:ident
        $(($($arg:ident: $type:ty),*$(,)?))?
        $({$($values:tt)*})?
    } => {
        impl RoutexRefreshClient {
            #[allow(clippy::too_many_arguments)]
            fn $service(
                &self,
                connection_data: &CxxVector<u8>,
                ticket: &CxxString,
                $($($arg: $type,)*)?
                session: &UniquePtr<CxxVector<u8>>,
            ) -> anyhow::Result<ffi::ServiceResult> {
                let mut request = self.inner.$service(
                    connection_data.as_slice().to_vec().into(),
                    &match ticket.to_string().parse() {
                        Ok(ticket) => ticket,
                        Err(err) => return Ok(err.into()),
                    },
                    $($($values)*)?
                );

                if let Some(session) = session.as_ref() {
                    request = request.session(session.as_slice().to_vec().into());
                }

                Ok(self.runtime.block_on(request.send()).into())
            }
        }
    }
}

service! {
    accounts
    (fields: &CxxVector<ffi::AccountField>, filter: &UniquePtr<ffi::AccountFilter>)
    {
        fields.into_iter().map(|f| (*f).try_into()).collect::<anyhow::Result<Vec<_>>>()?,
        filter.as_ref().map(TryInto::try_into).transpose()?,
    }
}

service! {
    balances
    (accounts: &CxxVector<ffi::AccountReference>)
    {
        accounts.into_iter().map(Into::into),
    }
}

service! {
    transactions
}
