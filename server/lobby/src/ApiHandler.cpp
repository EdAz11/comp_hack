/**
 * @file server/lobby/src/ApiHandler.cpp
 * @ingroup lobby
 *
 * @author COMP Omega <compomega@tutanota.com>
 *
 * @brief Civet API handler for the RESTful API.
 *
 * This file is part of the Lobby Server (lobby).
 *
 * Copyright (C) 2012-2017 COMP_hack Team <compomega@tutanota.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ApiHandler.h"

// libcomp Includes
#include <BaseServer.h>
#include <CString.h>
#include <DatabaseConfigMariaDB.h>
#include <DatabaseConfigSQLite3.h>
#include <Decrypt.h>
#include <Log.h>

using namespace lobby;

#define MAX_PAYLOAD (4096)

void ApiSession::Reset()
{
    username.Clear();
    challenge.Clear();
    account.reset();
}

ApiHandler::ApiHandler(const std::shared_ptr<objects::LobbyConfig>& config,
    const std::shared_ptr<lobby::LobbyServer>& server) : mConfig(config),
    mServer(server)
{
    mParsers["/auth/get_challenge"] = &ApiHandler::Auth_Token;
    mParsers["/account/get_cp"] = &ApiHandler::Account_GetCP;
    mParsers["/account/get_details"] = &ApiHandler::Account_GetDetails;
    mParsers["/account/register"] = &ApiHandler::Account_Register;
}

ApiHandler::~ApiHandler()
{
}

bool ApiHandler::Auth_Token(const JsonBox::Object& request,
    JsonBox::Object& response, const std::shared_ptr<ApiSession>& session)
{
    auto it = request.find("username");

    if(it == request.end())
    {
        session->Reset();
        return false;
    }

    libcomp::String username = it->second.getString();

    // Make sure the username did not change.
    if(!session->username.IsEmpty() && session->username != username)
    {
        session->Reset();
    }

    // Grab a new database connection.
    auto db = GetDatabase();

    // Sanity in an insane world.
    if(!db)
    {
        session->Reset();
        return false;
    }

    // Find the account for the given username.
    session->account = objects::Account::LoadAccountByUsername(
        db, username);

    // We must have a valid account for this to work.
    if(!session->account)
    {
        session->Reset();
        return false;
    }

    libcomp::String challenge = libcomp::Decrypt::GenerateRandom(10);

    // Save the challenge.
    session->username = username;
    session->challenge = challenge;

    response["challenge"] = JsonBox::Value(challenge.ToUtf8());
    response["salt"] = JsonBox::Value(session->account->GetSalt().ToUtf8());

    return true;
}

bool ApiHandler::Account_GetCP(const JsonBox::Object& request,
    JsonBox::Object& response, const std::shared_ptr<ApiSession>& session)
{
    (void)request;
    
    // Find the account for the given username.
    auto account = objects::Account::LoadAccountByUsername(
        GetDatabase(), session->username);

    if(!account)
    {
        return false;
    }

    response["cp"] = (int)account->GetCP();

    return true;
}

bool ApiHandler::Account_GetDetails(const JsonBox::Object& request,
    JsonBox::Object& response, const std::shared_ptr<ApiSession>& session)
{
    (void)request;

    // Find the account for the given username.
    auto account = objects::Account::LoadAccountByUsername(
        GetDatabase(), session->username);

    if(!account)
    {
        return false;
    }

    response["cp"] = (int)account->GetCP();
    response["username"] = account->GetUsername().ToUtf8();
    response["disp_name"] = account->GetDisplayName().ToUtf8();
    response["email"] = account->GetEmail().ToUtf8();
    response["ticket_count"] = (int)account->GetTicketCount();
    response["user_level"] = (int)account->GetUserLevel();
    response["enabled"] = account->GetEnabled();
    response["gm"] = account->GetIsGM();
    response["last_login"] = (int)account->GetLastLogin();
    response["character_count"] = (int)account->CharactersCount();

    return true;
}

bool ApiHandler::Account_Register(const JsonBox::Object& request,
    JsonBox::Object& response, const std::shared_ptr<ApiSession>& session)
{
    (void)session;

    libcomp::String username, email, password;

    auto it = request.find("username");

    if(it != request.end())
    {
        username = it->second.getString();
    }

    it = request.find("email");

    if(it != request.end())
    {
        email = it->second.getString();
    }

    it = request.find("password");

    if(it != request.end())
    {
        password = it->second.getString();
    }

    if(username.IsEmpty() || email.IsEmpty() || password.IsEmpty())
    {
        return false;
    }

    if(!username.Matches("^[a-z][a-z0-9]{3,31}$"))
    {
        response["error"] = "Bad username";

        return true;
    }
    else if(!password.Matches("^[a-zA-Z0-9\\\\\\(\\)\\[\\]\\/{}~`'\"<>"
        ".,_|!@#$%^&*+=-]{6,16}$"))
    {
        response["error"] = "Bad password";

        return true;
    }
    else if(!email.Matches("(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!"
        "#$%&'*+/=?^_`{|}~-]+)*|\"(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21"
        "\\x23-\\x5b\\x5d-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])*\")"
        "@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*"
        "[a-z0-9])?|\\[(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
        "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?|[a-z0-9-]*[a-z0-9]:(?:["
        "\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21-\\x5a\\x53-\\x7f]|\\\\[\\x01"
        "-\\x09\\x0b\\x0c\\x0e-\\x7f])+)\\])"))
    {
        // RFC 5322
        response["error"] = "Bad email";

        return true;
    }

    auto db = GetDatabase();

    if(objects::Account::LoadAccountByUsername(db, username) ||
        objects::Account::LoadAccountByEmail(db, email))
    {
        response["error"] = "Account exists";

        return true;
    }

    std::shared_ptr<objects::Account> account(new objects::Account);

    libcomp::String displayName = username;
    libcomp::String salt = libcomp::Decrypt::GenerateRandom(10);
    uint32_t cp = 1000000;
    uint8_t ticketCount = 20;
    int32_t userLevel = 1000;
    bool enabled = true;
    bool isGM = true;

    // Hash the password for database storage.
    password = libcomp::Decrypt::HashPassword(password, salt);

    account->SetUsername(username);
    account->SetDisplayName(displayName);
    account->SetEmail(email);
    account->SetPassword(password);
    account->SetSalt(salt);
    account->SetIsGM(isGM);
    account->SetCP(cp);
    account->SetTicketCount(ticketCount);
    account->SetUserLevel(userLevel);
    account->SetEnabled(enabled);
    account->Register(std::dynamic_pointer_cast<
        libcomp::PersistentObject>(account));

    if(!account->Insert(db))
    {
        response["error"] = "Failed to create account.";
    }
    else
    {
        response["error"] = "Success";
    }

    return true;
}

bool ApiHandler::Authenticate(const JsonBox::Object& request,
    JsonBox::Object& response,
    const std::shared_ptr<ApiSession>& session)
{
    // Check first if a challenge was ever requested.
    if(session->username.IsEmpty() || !session->account)
    {
        return false;
    }

    // Check for the challenge reply.
    auto it = request.find("challenge");

    if(it == request.end())
    {
        // Force the client to re-authenticate.
        session->Reset();
        return false;
    }

    libcomp::String challenge = it->second.getString();

    // Calculate the correct response.
    libcomp::String validChallenge = libcomp::Decrypt::HashPassword(
        session->account->GetPassword(), session->challenge);

    // Check the challenge.
    if(challenge != validChallenge)
    {
        // Force the client to re-authenticate.
        session->Reset();
        return false;
    }

    // Generate a new challenge.
    challenge = libcomp::Decrypt::GenerateRandom(10);
    session->challenge = challenge;

    response["challenge"] = JsonBox::Value(challenge.ToUtf8());

    return true;
}

std::shared_ptr<libcomp::Database> ApiHandler::GetDatabase() const
{
    libcomp::EnumMap<objects::ServerConfig::DatabaseType_t,
        std::shared_ptr<objects::DatabaseConfig>> configMap;

    configMap[objects::ServerConfig::DatabaseType_t::SQLITE3]
        = mConfig->GetSQLite3Config();

    configMap[objects::ServerConfig::DatabaseType_t::MARIADB]
        = mConfig->GetMariaDBConfig();

    auto dbType = mConfig->GetDatabaseType();
    auto db = libcomp::BaseServer::GetDatabase(dbType, configMap);

    if(db && !db->Use())
    {
        return {};
    }

    return db;
}

bool ApiHandler::handlePost(CivetServer *pServer,
    struct mg_connection *pConnection)
{
    (void)pServer;

    const mg_request_info *pRequestInfo = mg_get_request_info(pConnection);

    // Sanity check the request info.
    if(nullptr == pRequestInfo)
    {
        return false;
    }

    libcomp::String method(pRequestInfo->request_uri);

    if("/api/" != method.Left(strlen("/api/")))
    {
        return false;
    }

    method = method.Mid(strlen("/api"));

    size_t postContentLength = static_cast<size_t>(
        pRequestInfo->content_length);

    // Sanity check the post content length.
    if(0 == postContentLength)
    {
        mg_printf(pConnection, "HTTP/1.1 411 Length Required\r\n"
            "Connection: close\r\n\r\n");

        return true;
    }

    // Make sure the post request is not too large.
    if(MAX_PAYLOAD < postContentLength)
    {
        mg_printf(pConnection, "HTTP/1.1 413 Payload Too Large\r\n"
            "Connection: close\r\n\r\n");

        return true;
    }

    // Allocate.
    char *szPostData = new char[postContentLength + 1];

    // Read the post data.
    postContentLength = static_cast<size_t>(mg_read(
        pConnection, szPostData, postContentLength));
    szPostData[postContentLength] = 0;

    JsonBox::Value request;
    request.loadFromString(szPostData);

    if(request.isNull() || !request.isObject())
    {
        mg_printf(pConnection, "HTTP/1.1 418 I'm a teapot\r\n"
            "Connection: close\r\n\r\n");

        return true;
    }

    delete[] szPostData;

    const JsonBox::Object& obj = request.getObject();

    std::stringstream ss;
    JsonBox::Object response;

    libcomp::String clientAddress(pRequestInfo->remote_addr);

    std::shared_ptr<ApiSession> session;

    {
        auto it = mSessions.find(clientAddress);

        if(it != mSessions.end())
        {
            session = it->second;
        }
        else
        {
            session = std::make_shared<ApiSession>();
            session->clientAddress = clientAddress;

            mSessions[clientAddress] = session;
        }
    }

    if("/auth/get_challenge" != method &&
        "/account/register" != method &&
        !Authenticate(obj, response, session))
    {
        mg_printf(pConnection, "HTTP/1.1 401 Unauthorized\r\n"
            "Connection: close\r\n\r\n");

        return true;
    }

    auto it = mParsers.find(method);

    if(it == mParsers.end())
    {
        mg_printf(pConnection, "HTTP/1.1 404 Not Found\r\n"
            "Connection: close\r\n\r\n");

        return true;
    }

    if(!it->second(*this, obj, response, session))
    {
        mg_printf(pConnection, "HTTP/1.1 400 Bad Request\r\n"
            "Connection: close\r\n\r\n");

        return true;
    }

    JsonBox::Value responseValue(response);
    responseValue.writeToStream(ss);

    mg_printf(pConnection, "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n%s", (uint32_t)ss.str().size(),
        ss.str().c_str());

    return true;
}
