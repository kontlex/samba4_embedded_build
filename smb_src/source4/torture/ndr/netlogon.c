/* 
   Unix SMB/CIFS implementation.
   test suite for netlogon ndr operations

   Copyright (C) Jelmer Vernooij 2007
   Copyright (C) Guenther Deschner 2011
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "torture/ndr/ndr.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "torture/ndr/proto.h"

static const uint8_t netrserverauthenticate3_in_data[] = {
  0xb0, 0x2e, 0x0a, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x5c, 0x00, 0x4e, 0x00, 0x41, 0x00,
  0x54, 0x00, 0x49, 0x00, 0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x44, 0x00,
  0x43, 0x00, 0x2e, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2e, 0x00, 0x42, 0x00, 0x41, 0x00, 0x53, 0x00,
  0x45, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0b, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x24, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0a, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x00, 0x00,
  0x68, 0x8e, 0x3c, 0xdf, 0x23, 0x02, 0xb1, 0x51, 0xff, 0xff, 0x07, 0x60
};

static bool netrserverauthenticate3_in_check(struct torture_context *tctx,
											struct netr_ServerAuthenticate3 *r)
{
	uint8_t cred_expected[8] = { 0x68, 0x8e, 0x3c, 0xdf, 0x23, 0x02, 0xb1, 0x51 };
	torture_assert_str_equal(tctx, r->in.server_name, "\\\\NATIVE-DC.NATIVE.BASE", "server name");
	torture_assert_str_equal(tctx, r->in.account_name, "NATIVE-2K$", "account name");
	torture_assert_int_equal(tctx, r->in.secure_channel_type, 2, "secure channel type");
	torture_assert_str_equal(tctx, r->in.computer_name, "NATIVE-2K", "computer name");
	torture_assert_int_equal(tctx, *r->in.negotiate_flags, 0x6007ffff, "negotiate flags");
	torture_assert_mem_equal(tctx, cred_expected, r->in.credentials->data, 8, "credentials");
	return true;
}

static const uint8_t netrserverauthenticate3_out_data[] = {
  0x22, 0x0c, 0x86, 0x8a, 0xe9, 0x92, 0x93, 0xc9, 0xff, 0xff, 0x07, 0x60,
  0x54, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static bool netrserverauthenticate3_out_check(struct torture_context *tctx,
											struct netr_ServerAuthenticate3 *r)
{
	uint8_t cred_expected[8] = { 0x22, 0x0c, 0x86, 0x8a, 0xe9, 0x92, 0x93, 0xc9 };
	torture_assert_mem_equal(tctx, cred_expected, r->out.return_credentials->data, 8, "return_credentials");
	torture_assert_int_equal(tctx, *r->out.negotiate_flags, 0x6007ffff, "negotiate flags");
	torture_assert_int_equal(tctx, *r->out.rid, 0x454, "rid");
	torture_assert_ntstatus_ok(tctx, r->out.result, "return code");
	
	return true;
}

static const uint8_t netrserverreqchallenge_in_data[] = {
  0xb0, 0x2e, 0x0a, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x5c, 0x00, 0x4e, 0x00, 0x41, 0x00,
  0x54, 0x00, 0x49, 0x00, 0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x44, 0x00,
  0x43, 0x00, 0x2e, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2e, 0x00, 0x42, 0x00, 0x41, 0x00, 0x53, 0x00,
  0x45, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0a, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x00, 0x00,
  0xa3, 0x2c, 0xa2, 0x95, 0x40, 0xcc, 0xb7, 0xbb
};

static bool netrserverreqchallenge_in_check(struct torture_context *tctx,
					    struct netr_ServerReqChallenge *r)
{
	uint8_t cred_expected[8] = { 0xa3, 0x2c, 0xa2, 0x95, 0x40, 0xcc, 0xb7, 0xbb };
	torture_assert_str_equal(tctx, r->in.server_name, "\\\\NATIVE-DC.NATIVE.BASE", "server name");
	torture_assert_str_equal(tctx, r->in.computer_name, "NATIVE-2K", "account name");
	torture_assert_mem_equal(tctx, cred_expected, r->in.credentials->data, 8, "credentials");

	return true;
}

static const uint8_t netrserverreqchallenge_out_data[] = {
  0x22, 0xfc, 0xc1, 0x17, 0xc0, 0xae, 0x27, 0x8e, 0x00, 0x00, 0x00, 0x00
};

static bool netrserverreqchallenge_out_check(struct torture_context *tctx,
					     struct netr_ServerReqChallenge *r)
{
	uint8_t cred_expected[8] = { 0x22, 0xfc, 0xc1, 0x17, 0xc0, 0xae, 0x27, 0x8e };
	torture_assert_mem_equal(tctx, cred_expected, r->out.return_credentials->data, 8, "return_credentials");
	torture_assert_ntstatus_ok(tctx, r->out.result, "return code");

	return true;
}

static const uint8_t netrlogonsamlogon_w2k_in_data[] = {
	0x00, 0x00, 0x02, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x5c, 0x00, 0x57, 0x00, 0x32, 0x00,
	0x4b, 0x00, 0x53, 0x00, 0x52, 0x00, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x02, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x00, 0x00, 0x00, 0x4d, 0x00, 0x54, 0x00, 0x48, 0x00, 0x45, 0x00,
	0x4c, 0x00, 0x45, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x02, 0x00, 0x08, 0xaf, 0x72, 0x50, 0xa0, 0x5b, 0x50, 0x19,
	0x02, 0xc3, 0x39, 0x4d, 0x0c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x10, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x0c, 0x00, 0x14, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xad, 0xde, 0x00, 0x00, 0xef, 0xbe, 0x00, 0x00,
	0x1a, 0x00, 0x1a, 0x00, 0x18, 0x00, 0x02, 0x00, 0x14, 0x00, 0x14, 0x00,
	0x1c, 0x00, 0x02, 0x00, 0x31, 0xeb, 0xf4, 0x68, 0x62, 0x93, 0xfe, 0x38,
	0x51, 0xc1, 0x1d, 0x41, 0x0a, 0xbd, 0x5d, 0xdf, 0xe3, 0x4f, 0x76, 0x7f,
	0x19, 0x12, 0xcd, 0xfe, 0x9c, 0x68, 0xed, 0x9b, 0x1e, 0x9c, 0x66, 0xf6,
	0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	0x57, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x44, 0x00, 0x4f, 0x00, 0x4d, 0x00,
	0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
	0x61, 0x00, 0x64, 0x00, 0x6d, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x69, 0x00,
	0x73, 0x00, 0x74, 0x00, 0x72, 0x00, 0x61, 0x00, 0x74, 0x00, 0x6f, 0x00,
	0x72, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x5c, 0x00, 0x6d, 0x00, 0x74, 0x00,
	0x68, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x65, 0x00, 0x6e, 0x00, 0x61, 0x00,
	0x06, 0x00
};

static bool netrlogonsamlogon_w2k_in_check(struct torture_context *tctx,
					   struct netr_LogonSamLogon *r)
{
	uint8_t credential_expected[8] = { 0x08, 0xaf, 0x72, 0x50, 0xa0, 0x5b, 0x50, 0x19 };
	uint8_t return_authenticator_expected[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	uint8_t lmpassword_expected[16] = { 0x31, 0xeb, 0xf4, 0x68, 0x62, 0x93, 0xfe, 0x38, 0x51, 0xc1, 0x1d, 0x41, 0x0a, 0xbd, 0x5d, 0xdf };
	uint8_t ntpassword_expected[16] = { 0xe3, 0x4f, 0x76, 0x7f, 0x19, 0x12, 0xcd, 0xfe, 0x9c, 0x68, 0xed, 0x9b, 0x1e, 0x9c, 0x66, 0xf6 };

	torture_assert_str_equal(tctx, r->in.server_name, "\\\\W2KSRV", "server_name");
	torture_assert_str_equal(tctx, r->in.computer_name, "MTHELENA", "computer_name");
	torture_assert_mem_equal(tctx, r->in.credential->cred.data, credential_expected, 8, "credential");
/*	torture_assert_int_equal(tctx, r->in.credential->timestamp, 0, "credential.timestamp"); */
	torture_assert_mem_equal(tctx, r->in.return_authenticator->cred.data, return_authenticator_expected, 8, "return_authenticator.cred.data");
	torture_assert_int_equal(tctx, r->in.return_authenticator->timestamp, 0, "return_authenticator.timestamp");
	torture_assert_int_equal(tctx, r->in.logon_level, NetlogonInteractiveInformation, "logon_level");
	torture_assert(tctx, r->in.logon, "logon NULL pointer");
	torture_assert(tctx, r->in.logon->password, "logon->password NULL pointer");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.domain_name.length, 12, "domain_name.length");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.domain_name.size, 12, "domain_name.size");
	torture_assert_str_equal(tctx, r->in.logon->password->identity_info.domain_name.string, "W2KDOM", "domain_name.string");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.parameter_control, 0, "parameter_control");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.logon_id_low, 0xdead, "logon_id_low");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.logon_id_high, 0xbeef, "logon_id_high");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.account_name.length, 26, "account_name.length");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.account_name.size, 26, "account_name.size");
	torture_assert_str_equal(tctx, r->in.logon->password->identity_info.account_name.string, "administrator", "account_name.string");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.workstation.length, 20, "workstation.length");
	torture_assert_int_equal(tctx, r->in.logon->password->identity_info.workstation.size, 20, "workstation.size");
	torture_assert_str_equal(tctx, r->in.logon->password->identity_info.workstation.string, "\\\\mthelena", "workstation.string");
	torture_assert_mem_equal(tctx, r->in.logon->password->lmpassword.hash, lmpassword_expected, 16, "lmpassword");
	torture_assert_mem_equal(tctx, r->in.logon->password->ntpassword.hash, ntpassword_expected, 16, "ntpassword");
	torture_assert_int_equal(tctx, r->in.validation_level, 6, "validation_level");

	return true;
}

static const uint8_t netrlogonsamlogon_w2k_out_data[] = {
	0x6c, 0xdb, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0xc0
};

static bool netrlogonsamlogon_w2k_out_check(struct torture_context *tctx,
					    struct netr_LogonSamLogon *r)
{
	uint8_t return_authenticator_expected[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	torture_assert_mem_equal(tctx, r->out.return_authenticator->cred.data, return_authenticator_expected, 8, "return_authenticator.cred.data");
	torture_assert_int_equal(tctx, r->out.return_authenticator->timestamp, 0, "return_authenticator.timestamp");
	torture_assert(tctx, r->out.validation, "validation NULL pointer");
	torture_assert(tctx, (r->out.validation->sam6 == NULL), "sam6 not NULL");
	torture_assert_int_equal(tctx, *r->out.authoritative, 1, "authoritative");
	torture_assert_ntstatus_equal(tctx, r->out.result, NT_STATUS_INVALID_INFO_CLASS, "unexpected result");

	return true;
}

struct torture_suite *ndr_netlogon_suite(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "netlogon");

	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerReqChallenge, netrserverreqchallenge_in_data, NDR_IN, netrserverreqchallenge_in_check );
	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerReqChallenge, netrserverreqchallenge_out_data, NDR_OUT, netrserverreqchallenge_out_check );

	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerAuthenticate3, netrserverauthenticate3_in_data, NDR_IN, netrserverauthenticate3_in_check );
	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerAuthenticate3, netrserverauthenticate3_out_data, NDR_OUT, netrserverauthenticate3_out_check );

	torture_suite_add_ndr_pull_fn_test(suite, netr_LogonSamLogon, netrlogonsamlogon_w2k_in_data, NDR_IN, netrlogonsamlogon_w2k_in_check );
#if 0
	/* samba currently fails to parse a validation level 6 samlogon reply
	 * from w2k and other servers - gd */
	torture_suite_add_ndr_pull_io_test(suite, netr_LogonSamLogon, netrlogonsamlogon_w2k_in_data, netrlogonsamlogon_w2k_out_data, netrlogonsamlogon_w2k_out_check);
#endif

	return suite;
}
