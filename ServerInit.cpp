/*
    Copyright (C) 2019-Present SKALE Labs

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
    along with sgxwallet. If not, see <https://www.gnu.org/licenses/>.

    @file ServerInit.cpp
    @author Stan Kladko
    @date 2019
*/

#include <memory>
#include <iostream>

#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libff/algebra/curves/alt_bn128/alt_bn128_init.hpp"
#include <libff/common/profiling.hpp>
#include "bls.h"
#include "leveldb/db.h"
#include <jsonrpccpp/server/connectors/httpserver.h>

#include "third_party/spdlog/spdlog.h"
#include <gmp.h>
#include <sgx_urts.h>
#include <unistd.h>


#include "ExitHandler.h"
#include "BLSPrivateKeyShareSGX.h"
#include "sgxwallet_common.h"
#include "third_party/intel/create_enclave.h"
#include "secure_enclave_u.h"
#include "third_party/intel/sgx_detect.h"
#include "sgxwallet.h"
#include "LevelDB.h"
#include "SGXWalletServer.h"
#include "SGXRegistrationServer.h"
#include "SGXInfoServer.h"
#include "SEKManager.h"
#include "CSRManagerServer.h"
#include "BLSCrypto.h"
#include "ServerInit.h"
#include "SGXException.h"
#include "zmq_src/ZMQServer.h"
#include "SGXWalletServer.hpp"

uint32_t enclaveLogLevel = 0;

using namespace std;

void systemHealthCheck() {
    string ulimit;
    try {
        ulimit = exec("/bin/bash -c \"ulimit -n\"");
    } catch (...) {
        spdlog::error("Execution of '/bin/bash -c ulimit -n' failed");
        throw SGXException(EXECUTION_ULIMIT_FAILED, "Execution of '/bin/bash -c ulimit -n' failed.");
    }
    int noFiles = strtol(ulimit.c_str(), NULL, 10);

    auto noUlimitCheck = getenv("NO_ULIMIT_CHECK") != nullptr;

    if (noFiles < 65535 && !noUlimitCheck) {
        string errStr =
                "sgxwallet requires setting Linux file descriptor limit to at least 65535 "
                "You current limit (ulimit -n) is less than 65535. \n Please set it to 65535:"
                "by editing /etc/systemd/system.conf"
                "and setting 'DefaultLimitNOFILE=65535'\n"
                "After that, restart sgxwallet";
        spdlog::error(errStr);
        throw SGXException(WRONG_ULIMIT, errStr);
    }
}

void initUserSpace() {

    libff::inhibit_profiling_counters = true;

    libff::init_alt_bn128_params();

    LevelDB::initDataFolderAndDBs();

#ifndef SGX_HW_SIM
    systemHealthCheck();
#endif

}

uint64_t initEnclave() {

#ifndef SGX_HW_SIM
    unsigned long support;
    support = get_sgx_support();
    if (!SGX_OK(support)) {
        sgx_support_perror(support);
        throw SGXException(COULD_NOT_INIT_ENCLAVE, "SGX is not supported or not enabled");
    }
#endif

    spdlog::info("SGX_DEBUG_FLAG = {}", SGX_DEBUG_FLAG);

    sgx_status_t status = SGX_SUCCESS;

    {

        WRITE_LOCK(sgxInitMutex);

        if (eid != 0) {
            if (sgx_destroy_enclave(eid) != SGX_SUCCESS) {
                spdlog::error("Could not destroy enclave");
            }
        }

        eid = 0;
        updated = 0;

        status = sgx_create_enclave_search(ENCLAVE_NAME, SGX_DEBUG_FLAG, &token,
                                           &updated, &eid, 0);

        if (status != SGX_SUCCESS) {
            if (status == SGX_ERROR_ENCLAVE_FILE_ACCESS) {
                spdlog::error("sgx_create_enclave: {}: file not found", ENCLAVE_NAME);
                spdlog::error("Did you forget to set LD_LIBRARY_PATH?");
            } else {
                spdlog::error("sgx_create_enclave_search failed {} {}", ENCLAVE_NAME, status);
            }
            throw SGXException(COULD_NOT_INIT_ENCLAVE, "Error initing enclave. Please re-check your enviroment.");
        }

        spdlog::info("Enclave created and started successfully");

        status = trustedEnclaveInit(eid, enclaveLogLevel);
    }

    if (status != SGX_SUCCESS) {
        spdlog::error("trustedEnclaveInit failed: {}", status);
        return status;
    }

    spdlog::info("Enclave libtgmp library and logging initialized successfully");

    return SGX_SUCCESS;
}

void initAll(uint32_t _logLevel, bool _checkCert,
             bool _checkZMQSig, bool _autoSign, bool _generateTestKeys, bool _checkKeyOwnership) {

    static atomic<bool> sgxServerInited(false);
    static mutex initMutex;
    enclaveLogLevel = _logLevel;

    lock_guard <mutex> lock(initMutex);

    if (sgxServerInited)
        return;

    try {

        cout << "Running sgxwallet version:" << SGXWalletServer::getVersion() << endl;

        CHECK_STATE(sgxServerInited != 1)
        sgxServerInited = 1;

        uint64_t counter = 0;

        uint64_t initResult = 0;
        while ((initResult = initEnclave()) != 0 && counter < 10) {
            sleep(1);
            counter++;
        }

        if (initResult != 0) {
            spdlog::error("Coult not init enclave");
        }

        initUserSpace();
        initSEK();

        SGXWalletServer::createCertsIfNeeded();

        if (useHTTPS) {
            spdlog::info("Initing JSON-RPC server over HTTPS");
            spdlog::info("Check client cert: {}", _checkCert);
            SGXWalletServer::initHttpsServer(_checkCert);
            spdlog::info("Inited JSON-RPC server over HTTPS");
        } else {
            spdlog::info("Initing JSON-RPC server over HTTP");
            SGXWalletServer::initHttpServer();
            spdlog::info("Inited JSON-RPC server over HTTP");
        }

        SGXRegistrationServer::initRegistrationServer(_autoSign);
        CSRManagerServer::initCSRManagerServer();
        SGXInfoServer::initInfoServer(_logLevel, _checkCert, _autoSign, _generateTestKeys);
        ZMQServer::initZMQServer(_checkZMQSig, _checkKeyOwnership);

        sgxServerInited = true;
    } catch (SGXException &_e) {
        spdlog::error(_e.getMessage());
        ExitHandler::exitHandler(SIGTERM, ExitHandler::ec_initing_user_space);
    } catch (exception &_e) {
        spdlog::error(_e.what());
        ExitHandler::exitHandler(SIGTERM, ExitHandler::ec_initing_user_space);
    }
    catch (...) {
        exception_ptr p = current_exception();
        printf("Exception %s \n", p.__cxa_exception_type()->name());
        spdlog::error("Unknown exception");
        ExitHandler::exitHandler(SIGTERM, ExitHandler::ec_initing_user_space);
    }
};

void exitAll() {
    SGXWalletServer::exitServer();
    SGXRegistrationServer::exitServer();
    CSRManagerServer::exitServer();
    SGXInfoServer::exitServer();
    ZMQServer::exitZMQServer();
}
