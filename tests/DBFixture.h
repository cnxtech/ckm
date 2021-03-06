/*
 *  Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Kyungwook Tak <k.tak@samsung.com>
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
 * @file        DBFixture.h
 * @author      Maciej Karpiuk (m.karpiuk2@samsung.com)
 * @version
 * @brief
 */
#pragma once

#include <test_common.h>
#include <ckm/ckm-type.h>
#include <protocols.h>
#include <chrono>

class DBFixture
{
    public:
        DBFixture();
        DBFixture(const char *db_fname);

        constexpr static const char* m_default_name = "name";
        constexpr static const char* m_default_label = "label";

        // ::::::::::::::::::::::::: helper methods :::::::::::::::::::::::::
        static void generate_name(unsigned int id, CKM::Name & output);
        static void generate_label(unsigned int id, CKM::Label & output);
        static CKM::DB::Row create_default_row(CKM::DataType type = CKM::DataType::BINARY_DATA);
        static CKM::DB::Row create_default_row(const CKM::Name &name,
                                             const CKM::Label &label,
                                             CKM::DataType type = CKM::DataType::BINARY_DATA);
        static void compare_row(const CKM::DB::Row &lhs, const CKM::DB::Row &rhs);

        // ::::::::::::::::::::::::: time measurement :::::::::::::::::::::::::
        void performance_start(const char *operation_name);
        void performance_stop(long num_operations_performed);

        // ::::::::::::::::::::::::: DB :::::::::::::::::::::::::
        void generate_perf_DB(unsigned int num_name, unsigned int num_label);
        long add_full_access_rights(unsigned int num_name, unsigned int num_names_per_label);
        void check_DB_integrity(const CKM::DB::Row &rowPattern);
        void insert_row();
        void insert_row(const CKM::Name &name, const CKM::Label &owner_label);
        void delete_row(const CKM::Name &name, const CKM::Label &owner_label);
        void add_permission(const CKM::Name &name, const CKM::Label &owner_label, const CKM::Label &accessor_label);
        void read_row_expect_success(const CKM::Name &name, const CKM::Label &owner_label);

        CKM::DB::Crypto    m_db;
    private:
        void    init();
        double  performance_get_time_elapsed_ms();

        constexpr static const char* m_crypto_db_fname = "/tmp/testme.db";
        std::string m_operation;
        std::chrono::high_resolution_clock::time_point m_start_time, m_end_time;
};
