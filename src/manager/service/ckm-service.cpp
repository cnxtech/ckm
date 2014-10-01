/*
 *  Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 *
 *
 * @file        ckm-service.cpp
 * @author      Bartlomiej Grzelewski (b.grzelewski@samsung.com)
 * @version     1.0
 * @brief       CKM service implementation.
 */
#include <service-thread.h>
#include <generic-socket-manager.h>
#include <connection-info.h>
#include <message-buffer.h>
#include <protocols.h>

#include <dpl/serialization.h>
#include <dpl/log/log.h>

#include <ckm-service.h>
#include <ckm-logic.h>

namespace {
const CKM::InterfaceID SOCKET_ID_CONTROL = 0;
const CKM::InterfaceID SOCKET_ID_STORAGE = 1;
} // namespace anonymous

namespace CKM {

CKMService::CKMService()
  : m_logic(new CKMLogic)
{}

CKMService::~CKMService() {
    delete m_logic;
}

GenericSocketService::ServiceDescriptionVector CKMService::GetServiceDescription()
{
    return ServiceDescriptionVector {
        {SERVICE_SOCKET_CKM_CONTROL, "key-manager::api-control", SOCKET_ID_CONTROL},
        {SERVICE_SOCKET_CKM_STORAGE, "key-manager::api-storage", SOCKET_ID_STORAGE}
    };
}

void CKMService::accept(const AcceptEvent &event) {
    LogDebug("Accept event");
    auto &info = m_connectionInfoMap[event.connectionID.counter];
    info.interfaceID = event.interfaceID;
    info.credentials = event.credentials;
}

void CKMService::write(const WriteEvent &event) {
    LogDebug("Write event (" << event.size << " bytes)");
}

void CKMService::process(const ReadEvent &event) {
    LogDebug("Read event");
    auto &info = m_connectionInfoMap[event.connectionID.counter];
    info.buffer.Push(event.rawBuffer);
    while(processOne(event.connectionID, info));
}

bool CKMService::processOne(
    const ConnectionID &conn,
    ConnectionInfo &info)
{
    LogDebug ("process One");
    RawBuffer response;

    Try {
        if (!info.buffer.Ready())
            return false;

        if (info.interfaceID == SOCKET_ID_CONTROL)
            response = processControl(info.buffer);
        else
            response = processStorage(info.credentials, info.buffer);

        m_serviceManager->Write(conn, response);

        return true;
    } Catch (MessageBuffer::Exception::Base) {
        LogError("Broken protocol. Closing socket.");
    } Catch (Exception::BrokenProtocol) {
        LogError("Broken protocol. Closing socket.");
    } catch (const std::string &e) {
        LogError("String exception(" << e << "). Closing socket");
    } catch (...) {
        LogError("Unknown exception. Closing socket.");
    }

    m_serviceManager->Close(conn);
    return false;
}

RawBuffer CKMService::processControl(MessageBuffer &buffer) {
    int command;
    int cc_mode_status;
    uid_t user;
    ControlCommand cc;
    Password newPass, oldPass;
    std::string smackLabel;

    Deserialization::Deserialize(buffer, command);

    LogDebug("Process control. Command: " << command);

    cc = static_cast<ControlCommand>(command);

    switch(cc) {
    case ControlCommand::UNLOCK_USER_KEY:
        Deserialization::Deserialize(buffer, user);
        Deserialization::Deserialize(buffer, newPass);
        return m_logic->unlockUserKey(user, newPass);
    case ControlCommand::LOCK_USER_KEY:
        Deserialization::Deserialize(buffer, user);
        return m_logic->lockUserKey(user);
    case ControlCommand::REMOVE_USER_DATA:
        Deserialization::Deserialize(buffer, user);
        return m_logic->removeUserData(user);
    case ControlCommand::CHANGE_USER_PASSWORD:
        Deserialization::Deserialize(buffer, user);
        Deserialization::Deserialize(buffer, oldPass);
        Deserialization::Deserialize(buffer, newPass);
        return m_logic->changeUserPassword(user, oldPass, newPass);
    case ControlCommand::RESET_USER_PASSWORD:
        Deserialization::Deserialize(buffer, user);
        Deserialization::Deserialize(buffer, newPass);
        return m_logic->resetUserPassword(user, newPass);
    case ControlCommand::REMOVE_APP_DATA:
        Deserialization::Deserialize(buffer, smackLabel);
        return m_logic->removeApplicationData(smackLabel);
    case ControlCommand::SET_CC_MODE:
        Deserialization::Deserialize(buffer, cc_mode_status);
        return m_logic->setCCModeStatus(static_cast<CCModeState>(cc_mode_status));
    case ControlCommand::ALLOW_ACCESS:
    {
        std::string owner;
        std::string item_alias;
        std::string accessor_label;
        int req_rights;

        Deserialization::Deserialize(buffer, user);
        Deserialization::Deserialize(buffer, owner);
        Deserialization::Deserialize(buffer, item_alias);
        Deserialization::Deserialize(buffer, accessor_label);
        Deserialization::Deserialize(buffer, req_rights);
        Credentials cred =
            {
                user,
                owner
            };
        return m_logic->allowAccess(
            cred,
            command,
            0, // dummy
            item_alias,
            accessor_label,
            static_cast<AccessRight>(req_rights));
    }
    case ControlCommand::DENY_ACCESS:
    {
        std::string owner;
        std::string item_alias;
        std::string accessor_label;

        Deserialization::Deserialize(buffer, user);
        Deserialization::Deserialize(buffer, owner);
        Deserialization::Deserialize(buffer, item_alias);
        Deserialization::Deserialize(buffer, accessor_label);
        Credentials cred =
            {
                user,
                owner
            };
        return m_logic->denyAccess(
            cred,
            command,
            0, // dummy
            item_alias,
            accessor_label);
    }
    default:
        Throw(Exception::BrokenProtocol);
    }
}

RawBuffer CKMService::processStorage(Credentials &cred, MessageBuffer &buffer)
{
    int command;
    int msgID;
    int tmpDataType;
    Alias alias;
    std::string user;

    Deserialization::Deserialize(buffer, command);
    Deserialization::Deserialize(buffer, msgID);

    // This is a workaround solution for locktype=None in Tizen 2.2.1
    // When locktype is None, lockscreen app doesn't interfere with unlocking process.
    // Therefor lockscreen app cannot notify unlock events to key-manager when locktype is None.
    // So, to unlock user data when lock type is None, key-manager always try to unlock user data with null password.
    // Even if the result is fail, it will be ignored.
    Password nullPassword("");
    m_logic->unlockUserKey(cred.uid, nullPassword);

    LogDebug("Process storage. Command: " << command);

    switch(static_cast<LogicCommand>(command)) {
        case LogicCommand::SAVE:
        {
            RawBuffer rawData;
            PolicySerializable policy;
            Deserialization::Deserialize(buffer, tmpDataType);
            Deserialization::Deserialize(buffer, alias);
            Deserialization::Deserialize(buffer, rawData);
            Deserialization::Deserialize(buffer, policy);
            return m_logic->saveData(
                cred,
                msgID,
                static_cast<DBDataType>(tmpDataType),
                alias,
                rawData,
                policy);
        }
        case LogicCommand::REMOVE:
        {
            Deserialization::Deserialize(buffer, tmpDataType);
            Deserialization::Deserialize(buffer, alias);
            return m_logic->removeData(
                cred,
                msgID,
                static_cast<DBDataType>(tmpDataType),
                alias);
        }
        case LogicCommand::GET:
        {
            Password password;
            Deserialization::Deserialize(buffer, tmpDataType);
            Deserialization::Deserialize(buffer, alias);
            Deserialization::Deserialize(buffer, password);
            return m_logic->getData(
                cred,
                msgID,
                static_cast<DBDataType>(tmpDataType),
                alias,
                password);
        }
        case LogicCommand::GET_LIST:
        {
            Deserialization::Deserialize(buffer, tmpDataType);
            return m_logic->getDataList(
                cred,
                msgID,
                static_cast<DBDataType>(tmpDataType));
        }
        case LogicCommand::CREATE_KEY_PAIR_RSA:
        case LogicCommand::CREATE_KEY_PAIR_DSA:
        case LogicCommand::CREATE_KEY_PAIR_ECDSA:
        {
            int additional_param;
            Alias privateKeyAlias;
            Alias publicKeyAlias;
            PolicySerializable policyPrivateKey;
            PolicySerializable policyPublicKey;
            Deserialization::Deserialize(buffer, additional_param);
            Deserialization::Deserialize(buffer, policyPrivateKey);
            Deserialization::Deserialize(buffer, policyPublicKey);
            Deserialization::Deserialize(buffer, privateKeyAlias);
            Deserialization::Deserialize(buffer, publicKeyAlias);
            return m_logic->createKeyPair(
                cred,
                static_cast<LogicCommand>(command),
                msgID,
                additional_param,
                privateKeyAlias,
                publicKeyAlias,
                policyPrivateKey,
                policyPublicKey);
        }
        case LogicCommand::GET_CHAIN_CERT:
        {
            RawBuffer certificate;
            RawBufferVector rawBufferVector;
            Deserialization::Deserialize(buffer, certificate);
            Deserialization::Deserialize(buffer, rawBufferVector);
            return m_logic->getCertificateChain(
                cred,
                msgID,
                certificate,
                rawBufferVector);
        }
        case LogicCommand::GET_CHAIN_ALIAS:
        {
            RawBuffer certificate;
            AliasVector aliasVector;
            Deserialization::Deserialize(buffer, certificate);
            Deserialization::Deserialize(buffer, aliasVector);
            return m_logic->getCertificateChain(
                cred,
                msgID,
                certificate,
                aliasVector);
        }
        case LogicCommand::CREATE_SIGNATURE:
        {
            Alias privateKeyAlias;
            Password password;        // password for private_key
            RawBuffer message;
            int padding, hash;
            Deserialization::Deserialize(buffer, privateKeyAlias);
            Deserialization::Deserialize(buffer, password);
            Deserialization::Deserialize(buffer, message);
            Deserialization::Deserialize(buffer, hash);
            Deserialization::Deserialize(buffer, padding);

            return m_logic->createSignature(
                  cred,
                  msgID,
                  privateKeyAlias,
                  password,           // password for private_key
                  message,
                  static_cast<HashAlgorithm>(hash),
                  static_cast<RSAPaddingAlgorithm>(padding));
        }
        case LogicCommand::VERIFY_SIGNATURE:
        {
            Alias publicKeyOrCertAlias;
            Password password;           // password for public_key (optional)
            RawBuffer message;
            RawBuffer signature;
            //HashAlgorithm hash;
            //RSAPaddingAlgorithm padding;
            int padding, hash;
            Deserialization::Deserialize(buffer, publicKeyOrCertAlias);
            Deserialization::Deserialize(buffer, password);
            Deserialization::Deserialize(buffer, message);
            Deserialization::Deserialize(buffer, signature);
            Deserialization::Deserialize(buffer, hash);
            Deserialization::Deserialize(buffer, padding);
            return m_logic->verifySignature(
                cred,
                msgID,
                publicKeyOrCertAlias,
                password,           // password for public_key (optional)
                message,
                signature,
                static_cast<const HashAlgorithm>(hash),
                static_cast<const RSAPaddingAlgorithm>(padding));
        }
        case LogicCommand::ALLOW_ACCESS:
        {
            Alias item_alias;
            std::string accessor_label;
            int req_rights;
            Deserialization::Deserialize(buffer, item_alias);
            Deserialization::Deserialize(buffer, accessor_label);
            Deserialization::Deserialize(buffer, req_rights);
            return m_logic->allowAccess(
                cred,
                command,
                msgID,
                item_alias,
                accessor_label,
                static_cast<AccessRight>(req_rights));
        }
        case LogicCommand::DENY_ACCESS:
        {
            Alias item_alias;
            std::string accessor_label;
            Deserialization::Deserialize(buffer, item_alias);
            Deserialization::Deserialize(buffer, accessor_label);
            return m_logic->denyAccess(
                cred,
                command,
                msgID,
                item_alias,
                accessor_label);
        }
        default:
            Throw(Exception::BrokenProtocol);
    }
}


void CKMService::close(const CloseEvent &event) {
    LogDebug("Close event");
    m_connectionInfoMap.erase(event.connectionID.counter);
}

} // namespace CKM

