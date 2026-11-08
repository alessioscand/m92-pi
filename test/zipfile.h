// m92-pi - ROM zips for the Linux tools (harness, hashrun): a ROM is found by
// CRC and size, then by name, as MAME does
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "miniz.h"
#include <cstring>
#include <string>

struct zipfile
{
	mz_zip_archive zip;
	bool ok = false;
	explicit zipfile(const std::string &path)
	{
		memset(&zip, 0, sizeof zip);
		ok = !path.empty() && mz_zip_reader_init_file(&zip, path.c_str(), 0);
	}
	~zipfile() { if (ok) mz_zip_reader_end(&zip); }
	zipfile(const zipfile &) = delete;
	zipfile &operator=(const zipfile &) = delete;

	bool fetch(const char *name, uint32_t crc, uint32_t size, uint8_t *dst)
	{
		if (!ok)
			return false;
		const int n = int(mz_zip_reader_get_num_files(&zip));
		int found = -1;
		for (int i = 0; i < n && found < 0; i++)
		{
			mz_zip_archive_file_stat st;
			if (mz_zip_reader_file_stat(&zip, i, &st) && st.m_crc32 == crc && st.m_uncomp_size == size)
				found = i;
		}
		if (found < 0)
			found = mz_zip_reader_locate_file(&zip, name, nullptr, 0);
		if (found < 0)
			return false;
		mz_zip_archive_file_stat st;
		if (!mz_zip_reader_file_stat(&zip, found, &st) || st.m_uncomp_size != size)
			return false;
		return mz_zip_reader_extract_to_mem(&zip, found, dst, size, 0);
	}
};
