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
 * @file        ckm-logic.h
 * @author      Bartlomiej Grzelewski (b.grzelewski@samsung.com)
 * @version     1.0
 * @brief       Sample service implementation.
 */
#pragma once

#include <string>
#include <vector>

#include <message-buffer.h>
#include <protocols.h>
#include <ckm/ckm-type.h>
#include <connection-info.h>
#include <db-crypto.h>
#include <key-provider.h>
#include <crypto-logic.h>
#include <certificate-store.h>
#include <file-lock.h>
#include <access-control.h>

namespace CKM {

struct UserData {
    UserData()
      : isMainDKEK(false)
      , isDKEKConfirmed(false)
    {}

    KeyProvider    keyProvider;
    DBCrypto       database;
    CryptoLogic    crypto;
    bool           isMainDKEK;
    bool           isDKEKConfirmed;
};

class CKMLogic {
public:
    CKMLogic();
    CKMLogic(const CKMLogic &) = delete;
    CKMLogic(CKMLogic &&) = delete;
    CKMLogic& operator=(const CKMLogic &) = delete;
    CKMLogic& operator=(CKMLogic &&) = delete;
    virtual ~CKMLogic();

    RawBuffer unlockUserKey(uid_t user, const Password &password, bool apiRequest = true);

    RawBuffer lockUserKey(uid_t user);

    RawBuffer removeUserData(uid_t user);

    RawBuffer changeUserPassword(
        uid_t user,
        const Password &oldPassword,
        const Password &newPassword);

    RawBuffer resetUserPassword(
        uid_t user,
        const Password &newPassword);

    RawBuffer removeApplicationData(
        const Label &smackLabel);

    RawBuffer saveData(
        const Credentials &cred,
        int commandId,
        const Name &name,
        const Label &label,
        const RawBuffer &data,
        DBDataType dataType,
        const PolicySerializable &policy);

    RawBuffer savePKCS12(
        const Credentials &cred,
        int commandId,
        const Name &name,
        const Label &label,
        const PKCS12Serializable &pkcs,
        const PolicySerializable &keyPolicy,
        const PolicySerializable &certPolicy);

    RawBuffer removeData(
        const Credentials &cred,
        int commandId,
        const Name &name,
        const Label &label);

    RawBuffer getData(
        const Credentials &cred,
        int commandId,
        DBDataType dataType,
        const Name &name,
        const Label &label,
        const Password &password);

    RawBuffer getPKCS12(
        const Credentials &cred,
        int commandId,
        const Name &name,
        const Label &label);

    RawBuffer getDataList(
        const Credentials &cred,
        int commandId,
        DBDataType dataType);

    RawBuffer createKeyPair(
        const Credentials &cred,
        LogicCommand protocol_cmd,
        int commandId,
        const int additional_param,
        const Name &namePrivate,
        const Label &labelPrivate,
        const Name &namePublic,
        const Label &labelPublic,
        const PolicySerializable &policyPrivate,
        const PolicySerializable &policyPublic);

    RawBuffer getCertificateChain(
        const Credentials &cred,
        int commandId,
        const RawBuffer &certificate,
        const RawBufferVector &untrustedCertificates);

    RawBuffer getCertificateChain(
        const Credentials &cred,
        int commandId,
        const RawBuffer &certificate,
        const LabelNameVector &labelNameVector);

    RawBuffer  createSignature(
        const Credentials &cred,
        int commandId,
        const Name &privateKeyName,
        const Label & ownerLabel,
        const Password &password,           // password for private_key
        const RawBuffer &message,
        const HashAlgorithm hash,
        const RSAPaddingAlgorithm padding);

    RawBuffer verifySignature(
        const Credentials &cred,
        int commandId,
        const Name &publicKeyOrCertName,
        const Label &label,
        const Password &password,           // password for public_key (optional)
        const RawBuffer &message,
        const RawBuffer &signature,
        const HashAlgorithm hash,
        const RSAPaddingAlgorithm padding);

    RawBuffer updateCCMode();

    RawBuffer setPermission(
        const Credentials &cred,
        const int command,
        const int msgID,
        const Name &name,
        const Label &label,
        const Label &accessor_label,
        const PermissionMask permissionMask);

private:

    void loadDKEKFile(
        uid_t user,
        const Password &password,
        bool apiReq);

    void chooseDKEKFile(
        UserData &handle,
        const Password &password,
        const RawBuffer &first,
        const RawBuffer &second);

    void saveDKEKFile(
        uid_t user,
        const Password &password);

    int verifyBinaryData(
        DBDataType dataType,
        const RawBuffer &input_data) const;

    int checkSaveConditions(
        const Credentials &cred,
        UserData &handler,
        const Name &name,
        const Label &label);

    int saveDataHelper(
        const Credentials &cred,
        const Name &name,
        const Label &label,
        DBDataType dataType,
        const RawBuffer &data,
        const PolicySerializable &policy);

    int saveDataHelper(
        const Credentials &cred,
        const Name &name,
        const Label &label,
        const PKCS12Serializable &pkcs,
        const PolicySerializable &keyPolicy,
        const PolicySerializable &certPolicy);

    DBRow createEncryptedDBRow(
        CryptoLogic &crypto,
        const Name &name,
        const Label &label,
        DBDataType dataType,
        const RawBuffer &data,
        const Policy &policy) const;

    int getPKCS12Helper(
        const Credentials &cred,
        const Name &name,
        const Label &label,
        KeyShPtr & privKey,
        CertificateShPtr & cert,
        CertificateShPtrVector & caChain);

    int extractPKCS12Data(
        CryptoLogic &crypto,
        const Name &name,
        const Label &ownerLabel,
        const PKCS12Serializable &pkcs,
        const PolicySerializable &keyPolicy,
        const PolicySerializable &certPolicy,
        DBRowVector &output) const;

    int removeDataHelper(
        const Credentials &cred,
        const Name &name,
        const Label &ownerLabel);

    int readSingleRow(
        const Name &name,
        const Label &ownerLabel,
        DBDataType dataType,
        DBCrypto & database,
        DBRow &row);

    int readMultiRow(const Name &name,
        const Label &ownerLabel,
        DBDataType dataType,
        DBCrypto & database,
        DBRowVector &output);

    int checkDataPermissionsHelper(
        const Name &name,
        const Label &ownerLabel,
        const Label &accessorLabel,
        const DBRow &row,
        bool exportFlag,
        DBCrypto & database);

    int readDataHelper(
        bool exportFlag,
        const Credentials &cred,
        DBDataType dataType,
        const Name &name,
        const Label &label,
        const Password &password,
        DBRow &row);

    int readDataHelper(
        bool exportFlag,
        const Credentials &cred,
        DBDataType dataType,
        const Name &name,
        const Label &label,
        const Password &password,
        DBRowVector &rows);

    int createKeyPairHelper(
        const Credentials &cred,
        const KeyType key_type,
        const int additional_param,
        const Name &namePrivate,
        const Label &labelPrivate,
        const Name &namePublic,
        const Label &labelPublic,
        const PolicySerializable &policyPrivate,
        const PolicySerializable &policyPublic);

    int getCertificateChainHelper(
        const Credentials &cred,
        const RawBuffer &certificate,
        const LabelNameVector &labelNameVector,
        RawBufferVector & chainRawVector);

    int setPermissionHelper(
        const Credentials &cred,
        const Name &name,
        const Label &ownerLabel,
        const Label &accessorLabel,
        const PermissionMask permissionMask);


    std::map<uid_t, UserData> m_userDataMap;
    CertificateStore m_certStore;
    AccessControl m_accessControl;
    //FileLock m_lock;
};

} // namespace CKM

