/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file WorkerThreadPool.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

class Agent;
class ZMQServer;

class WorkerThreadPool {

    void createThread( uint64_t threadNumber );

    recursive_mutex m;

protected:

    atomic_bool joined;
    vector<shared_ptr<thread>> threadpool;

    uint64_t numThreads = 0;
    ZMQServer* agent = nullptr;

public:

    WorkerThreadPool(uint64_t _numThreads, ZMQServer *_agent);

    virtual ~WorkerThreadPool();

    void joinAll();

    bool isJoined() const;

};
