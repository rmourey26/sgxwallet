/*
    Copyright (C) 2020-Present SKALE Labs

    This file is part of sgxwallet.

    sgxwallet is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sgxwallet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with sgxwallet.  If not, see <https://www.gnu.org/licenses/>.

    @file SGXInfoServer.cpp
    @author Oleh Nikolaiev
    @date 2020
*/

#include <iostream>
#include <fstream>
#include <sstream>

#include <jsonrpccpp/server/connectors/httpserver.h>

#include <stdio.h>

#include "sgxwallet_common.h"

#include "SGXException.h"
#include "LevelDB.h"

#include "SGXInfoServer.h"
#include "LevelDB.h"

#include "Log.h"
#include "common.h"

shared_ptr <SGXInfoServer> SGXInfoServer::server = nullptr;
shared_ptr <HttpServer> SGXInfoServer::httpServer = nullptr;

SGXInfoServer::SGXInfoServer(AbstractServerConnector &connector, serverVersion_t type)
        : AbstractInfoServer(connector, type) {}

Json::Value SGXInfoServer::getAllKeysInfo() {
    Json::Value result;

    try {
        result["allKeys"] = LevelDB::getLevelDb()->getAllKeys().str();
    } HANDLE_SGX_EXCEPTION(result)

    RETURN_SUCCESS(result)
}

Json::Value SGXInfoServer::getLastCreatedKey() {
    Json::Value result;

    try {
        pair<string, uint64_t> key = LevelDB::getLevelDb()->getLastCreatedKey();
        result["keyName"] = key.first;
        result["creationTime"] = key.second;
    } HANDLE_SGX_EXCEPTION(result)

    RETURN_SUCCESS(result)
}

Json::Value SGXInfoServer::getServerConfiguration() {
    Json::Value result;

    try {
        result["autoConfirm"] = false;
        result["logLevel"] = false;
        result["enclaveLogLevel"] = false;
        result["backupOption"] = false;
        result["HTTPS"] = false;
        result["autoSign"] = false;
        result["createTestKeys"] = false;
    } HANDLE_SGX_EXCEPTION(result)

    RETURN_SUCCESS(result)
}

Json::Value SGXInfoServer::isKeyExist(const string& key) {
    Json::Value result;

    result["isExists"] = false;
    try {
        shared_ptr <string> keyPtr = LevelDB::getLevelDb()->readString(key);

        if (keyPtr != nullptr) {
            result["IsExist"] = true;
        }
    } HANDLE_SGX_EXCEPTION(result)

    RETURN_SUCCESS(result)
}

int SGXInfoServer::initInfoServer() {
    httpServer = make_shared<HttpServer>(BASE_PORT + 5);
    server = make_shared<SGXInfoServer>(*httpServer, JSONRPC_SERVER_V2); // hybrid server (json-rpc 1.0 & 2.0)

    if (!server->StartListening()) {
        spdlog::error("Info server could not start listening on port {}", BASE_PORT + 5);
        exit(-10);
    } else {
        spdlog::info("Info server started on port {}", BASE_PORT + 5);
    }

    return 0;
}

shared_ptr<SGXInfoServer> SGXInfoServer::getServer() {
    CHECK_STATE(server);
    return server;
}