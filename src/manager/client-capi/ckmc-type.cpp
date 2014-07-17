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
 * @file        ckmc-type.h
 * @author      Yuseok Jeon(yuseok.jeon@samsung.com)
 * @version     1.0
 * @brief       new and free methods for the struct of CAPI
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ckm/ckm-type.h>
#include <ckmc/ckmc-type.h>
#include <ckmc/ckmc-error.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

int _ckmc_load_cert_from_x509(X509 *xCert, ckmc_cert **cert);


KEY_MANAGER_CAPI
ckmc_key *ckmc_key_new(unsigned char *raw_key, size_t key_size, ckmc_key_type key_type, char *password)
{
	ckmc_key *pkey = new ckmc_key;
	if(pkey == NULL)
		return NULL;

	pkey->raw_key = reinterpret_cast<unsigned char*>(malloc(key_size));
	if(pkey->raw_key == NULL) {
		free(pkey);
		return NULL;
	}
	memcpy(pkey->raw_key, raw_key, key_size);

	pkey->key_size = key_size;
	pkey->key_type = key_type;

	if(password != NULL) {
		pkey->password = reinterpret_cast<char*>(malloc(strlen(password) +1));
		if(pkey->password == NULL) {
			free(pkey);
			free(pkey->raw_key);
			return NULL;
		}
		memset(pkey->password, 0, strlen(password) +1);
		strncpy(pkey->password, password, strlen(password));
	}else {
		pkey->password = NULL;
	}

	return pkey;
}

KEY_MANAGER_CAPI
void ckmc_key_free(ckmc_key *key)
{
	if(key == NULL)
		return;

	if(key->password != NULL)
		free(key->password);
	if(key->raw_key != NULL) {
		memset(key->raw_key, 0, key->key_size);
		free(key->raw_key);
	}

	free(key);
}

KEY_MANAGER_CAPI
ckmc_raw_buffer * ckmc_buffer_new(unsigned char *data, size_t size)
{
	ckmc_raw_buffer *pbuff = new ckmc_raw_buffer;
	if(pbuff == NULL)
			return NULL;

	pbuff->data = reinterpret_cast<unsigned char*>(malloc(size));
	if(pbuff->data == NULL) {
		free(pbuff);
		return NULL;
	}
	memcpy(pbuff->data, data, size);

	pbuff->size = size;

	return pbuff;
}

KEY_MANAGER_CAPI
void ckmc_buffer_free(ckmc_raw_buffer *buffer)
{
	if(buffer == NULL)
		return;

	if(buffer->data != NULL) {
		memset(buffer->data, 0, buffer->size);
		free(buffer->data);
	}
	free(buffer);
}

KEY_MANAGER_CAPI
ckmc_cert *ckmc_cert_new(unsigned char *raw_cert, size_t cert_size, ckmc_data_format data_format)
{
	ckmc_cert *pcert = new ckmc_cert;
	if(pcert == NULL)
		return NULL;

	pcert->raw_cert = reinterpret_cast<unsigned char*>(malloc(cert_size));
	if(pcert->raw_cert == NULL) {
		free(pcert);
		return NULL;
	}
	memcpy(pcert->raw_cert, raw_cert, cert_size);

	pcert->cert_size = cert_size;
	pcert->data_format = data_format;

	return pcert;
}

KEY_MANAGER_CAPI
int ckmc_load_cert_from_file(const char *file_path, ckmc_cert **cert)
{
	OpenSSL_add_all_algorithms();

	FILE *fp = fopen(file_path, "r");
	if(fp == NULL)
		return CKMC_API_ERROR_FILE_ACCESS_DENIED;
	X509 *pcert = NULL;
	if(!(pcert = d2i_X509_fp(fp, NULL))) {
		fseek(fp, 0, SEEK_SET);
		pcert = PEM_read_X509(fp, NULL, NULL, NULL);
	}
	fclose(fp);
	if(pcert == NULL) {
		return CKMC_API_ERROR_INVALID_FORMAT;
	}

	int ret = _ckmc_load_cert_from_x509(pcert, cert);
	if(ret != CKMC_API_SUCCESS) {
		X509_free(pcert);
	}
	return ret;
}

KEY_MANAGER_CAPI
int ckmc_load_from_pkcs12_file(const char *file_path, const char *passphrase, ckmc_key **private_key, ckmc_cert **ckmcert, ckmc_cert_list **ca_cert_list)
{
	class Pkcs12Converter {
	private:
		FILE* fp_in;
		PKCS12* p12;
		EVP_PKEY* pkey;
		X509* x509Cert;
		STACK_OF(X509)* ca;

		int ret;
	public:
		ckmc_key *retPrivateKey;
		ckmc_cert *retCkmCert;
		ckmc_cert_list *retCaCertList;

		Pkcs12Converter(){
			fp_in = NULL;
			p12 = NULL;
			pkey = NULL;
			x509Cert = NULL;
			ca = NULL;
			ret = CKMC_API_SUCCESS;
			retPrivateKey = NULL;
			retCkmCert = NULL;
			retCaCertList = NULL;
		};
		~Pkcs12Converter(){
			if(fp_in != NULL)
				fclose(fp_in);
			if(p12 != NULL)
				PKCS12_free(p12);
			if(x509Cert != NULL)
				X509_free(x509Cert);
			if(pkey != NULL)
				EVP_PKEY_free(pkey);
			if(ca != NULL)
				sk_X509_pop_free(ca, X509_free);
			EVP_cleanup();

			if(ret != CKMC_API_SUCCESS) {
				if(retPrivateKey != NULL)
					ckmc_key_free(retPrivateKey);
				if(retCkmCert != NULL)
					ckmc_cert_free(retCkmCert);
				if(retCaCertList != NULL)
					ckmc_cert_list_all_free(retCaCertList);
			}
		};

		int parsePkcs12(const char *filePath, const char *pass) {
			fp_in = NULL;
			if(!(fp_in = fopen(filePath, "rb"))) {
				return CKMC_API_ERROR_FILE_ACCESS_DENIED;
			}

			if(!(p12 = d2i_PKCS12_fp(fp_in, NULL))) {
				return CKMC_API_ERROR_INVALID_FORMAT;
			}

			/* parse PKCS#12 certificate */
			if((ret = PKCS12_parse(p12, pass, &pkey, &x509Cert, &ca)) != 1) {
				return CKMC_API_ERROR_INVALID_FORMAT;
			}
			return CKMC_API_SUCCESS;
		}

		int toCkmCert() {
			if( (ret =_ckmc_load_cert_from_x509(x509Cert,&retCkmCert)) != CKMC_API_SUCCESS) {
				return ret;
			}
			return CKMC_API_SUCCESS;
		}

		int toCkmKey() {
			BIO *bkey = BIO_new(BIO_s_mem());

			i2d_PrivateKey_bio(bkey, pkey);

		    CKM::RawBuffer output(8196);
		    int size = BIO_read(bkey, output.data(), output.size());
			BIO_free_all(bkey);
		    if (size <= 0) {
		        return CKMC_API_ERROR_INVALID_FORMAT;
		    }
		    output.resize(size);

			int type = EVP_PKEY_type(pkey->type);
			ckmc_key_type key_type = CKMC_KEY_NONE;
			switch(type) {
			case EVP_PKEY_RSA :
				key_type = CKMC_KEY_RSA_PRIVATE;
				break;
			case EVP_PKEY_EC :
				key_type = CKMC_KEY_ECDSA_PRIVATE;
				break;
			}
			if(key_type == CKMC_KEY_NONE) {
				return CKMC_API_ERROR_INVALID_FORMAT;
			}

			char *nullPassword = NULL;

			retPrivateKey = ckmc_key_new(output.data(), size, key_type, nullPassword);

			return CKMC_API_SUCCESS;
		}

		int toCaCkmCertList() {
			X509* popedCert = NULL;
			ckmc_cert *popedCkmCert = NULL;
			ckmc_cert_list *tmpCertList = NULL;
			while((popedCert = sk_X509_pop(ca)) != NULL) {
				if( (ret =_ckmc_load_cert_from_x509(popedCert, &popedCkmCert)) != CKMC_API_SUCCESS) {
					return CKMC_API_ERROR_OUT_OF_MEMORY;
				}
				if(tmpCertList == NULL) { // first
					tmpCertList = ckmc_cert_list_new(popedCkmCert);
					retCaCertList = tmpCertList;
				}else {
					tmpCertList = ckmc_cert_list_add(tmpCertList, popedCkmCert);
				}
			}
			return CKMC_API_SUCCESS;
		}

	};

	int ret = CKMC_API_SUCCESS;

	Pkcs12Converter converter;
	if((ret = converter.parsePkcs12(file_path, passphrase)) != CKMC_API_SUCCESS) {
		return ret;
	}
	if((ret = converter.toCkmCert()) != CKMC_API_SUCCESS) {
		return ret;
	}
	if((ret = converter.toCkmKey()) != CKMC_API_SUCCESS) {
		return ret;
	}
	if((ret = converter.toCaCkmCertList()) != CKMC_API_SUCCESS) {
		return ret;
	}

	*private_key = converter.retPrivateKey;
	*ckmcert = converter.retCkmCert;
	*ca_cert_list = converter.retCaCertList;

	return CKMC_API_SUCCESS;
}

KEY_MANAGER_CAPI
void ckmc_cert_free(ckmc_cert *cert)
{
	if(cert == NULL)
		return;

	if(cert->raw_cert != NULL) {
		memset(cert->raw_cert, 0, cert->cert_size);
		free(cert->raw_cert);
	}
	free(cert);
}

KEY_MANAGER_CAPI
ckmc_alias_list *ckmc_alias_list_new(char *alias)
{
	ckmc_alias_list *previous = NULL;
	return ckmc_alias_list_add(previous, alias);
}

KEY_MANAGER_CAPI
ckmc_alias_list *ckmc_alias_list_add(ckmc_alias_list *previous, char *alias)
{
	ckmc_alias_list *plist = new ckmc_alias_list;

	plist->alias = alias;
	plist->next = NULL;

	if(previous != NULL)
		previous->next = plist;

	return plist;
}

KEY_MANAGER_CAPI
void ckmc_alias_list_free(ckmc_alias_list *first)
{
	if(first == NULL)
		return;

	ckmc_alias_list *current = NULL;
	ckmc_alias_list *next = first;
	do {
		current = next;
		next = current->next;
		free(current);
	}while(next != NULL);
}

KEY_MANAGER_CAPI
void ckmc_alias_list_all_free(ckmc_alias_list *first)
{
	if(first == NULL)
		return;
	ckmc_alias_list *current = NULL;
	ckmc_alias_list *next = first;
	do {
		current = next;
		next = current->next;
		if((current->alias)!=NULL) {
			free(current->alias);
		}
		free(current);
	}while(next != NULL);
}

KEY_MANAGER_CAPI
ckmc_cert_list *ckmc_cert_list_new(ckmc_cert *cert)
{
	ckmc_cert_list *previous = NULL;
	return ckmc_cert_list_add(previous, cert);
}

KEY_MANAGER_CAPI
ckmc_cert_list *ckmc_cert_list_add(ckmc_cert_list *previous, ckmc_cert *cert)
{
	ckmc_cert_list *plist = new ckmc_cert_list;

	plist->cert = cert;
	plist->next = NULL;

	if(previous != NULL)
		previous->next = plist;

	return plist;
}

KEY_MANAGER_CAPI
void ckmc_cert_list_free(ckmc_cert_list *first)
{
	if(first == NULL)
		return;

	ckmc_cert_list *current = NULL;
	ckmc_cert_list *next = first;
	do {
		current = next;
		next = current->next;
		free(current);
	}while(next != NULL);
}

KEY_MANAGER_CAPI
void ckmc_cert_list_all_free(ckmc_cert_list *first)
{
	if(first == NULL)
		return;

	ckmc_cert_list *current = NULL;
	ckmc_cert_list *next = first;
	do {
		current = next;
		next = current->next;
		if((current->cert)!=NULL) {
			ckmc_cert_free(current->cert);
		}
		free(current);
	}while(next != NULL);
}

int _ckmc_load_cert_from_x509(X509 *xCert, ckmc_cert **cert)
{
	if(xCert == NULL) {
		return CKMC_API_ERROR_INVALID_FORMAT;
	}

	BIO *bcert = BIO_new(BIO_s_mem());

	i2d_X509_bio(bcert, xCert);

    CKM::RawBuffer output(8196);
    int size = BIO_read(bcert, output.data(), output.size());
	BIO_free_all(bcert);
    if (size <= 0) {
        return CKMC_API_ERROR_INVALID_FORMAT;
    }
    output.resize(size);

	*cert = ckmc_cert_new(output.data(), output.size(), CKMC_FORM_DER);

	return CKMC_API_SUCCESS;
}