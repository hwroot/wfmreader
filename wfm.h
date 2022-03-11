#pragma once
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

#define TEK_WFM_UPDATE_OFFSET		0x310
#define TEK_WFM_CURVE_INFO			30
#define TEK_WFM_UPDATE_SPEC			24

//
// http://download.tek.com/manual/Waveform-File-Format-Manual-077022011.pdf
// All MSO 5 and MSO 6
//
#pragma pack(push, 1)

typedef struct
{
	uint16_t byte_order;		// 0f 0f - intel, f0 f0 - ppc
	uint8_t  ver[8];			// WFM#003
	uint8_t  nbyte_cnt;
	uint32_t nbytes_to_eof;
	uint8_t  point_size;		// sample size
	uint32_t curve_buf_off;	// offset to curve buffer from beginning of the file	
	uint8_t  unused1[0x34];		// other stuff not important
	uint32_t nfast_frames;		// Number of FastFrames - 1
	uint32_t wfm_hdr_size;
} tek_wfm_file_info;

typedef struct _tek_wfm_curve_info
{
	uint32_t state_flag;
	uint32_t chksum_type;
	uint16_t chksum;
	uint32_t pre_chg_off;
	uint32_t data_start_off;
	uint32_t post_chg_off;
	uint32_t post_chg_stop_off;
	uint32_t end_of_crv_buf_off;
} tek_wfm_curve_info;

#pragma pack(pop)

class wfm_reader
{
private:
	int m_fd;
	uint32_t				m_curve_size;
	tek_wfm_file_info		wfm_info;
	tek_wfm_curve_info		wfm_curve;	
public:
	wfm_reader()
	{
		m_fd = 0;
		wfm_info = { 0 };
		wfm_curve = { 0 };
	}
	~wfm_reader()
	{
		if (m_fd != 0)
		{
			_close(m_fd);
		}		
	}
	bool open_wfm(const char *file_path)
	{
		int nread;
		errno_t err;
		err = _sopen_s(&m_fd, file_path, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IREAD);

		if (err == 0)
		{
			nread = _read(m_fd, &wfm_info, sizeof(tek_wfm_file_info) );

			if (nread != sizeof(tek_wfm_file_info))
				return false;

			_lseek(m_fd, TEK_WFM_UPDATE_OFFSET + TEK_WFM_UPDATE_SPEC, SEEK_SET );

			nread = _read(m_fd, &wfm_curve, TEK_WFM_CURVE_INFO);

			if (nread != TEK_WFM_CURVE_INFO)
				return false;

			m_curve_size = ( wfm_curve.post_chg_off - wfm_curve.data_start_off ) / wfm_info.point_size;

			return true;			

		}		

		return false;
	}
	uint32_t count()
	{
		return wfm_info.nfast_frames + 1;
	}
	uint8_t point_size()
	{
		return wfm_info.point_size;
	}
	// curve size in points
	uint32_t curve_size()
	{
		return m_curve_size;
	}
	bool get_curve_data(uint32_t n_frame, uint8_t *buffer, uint32_t start_point, uint32_t npoints )
	{
		int nbytes;
		uint32_t nsize = npoints * wfm_info.point_size;

		if (n_frame > wfm_info.nfast_frames || 
			m_curve_size < start_point + npoints)
			return false;

		long long file_offset = wfm_info.curve_buf_off;											// get offset to array of all curves
		file_offset += wfm_curve.end_of_crv_buf_off * n_frame + wfm_curve.data_start_off;		// calculate frame offset
		file_offset += start_point * wfm_info.point_size;										// add offset within a frame

		if (_lseeki64(m_fd, file_offset, SEEK_SET) != file_offset)
			return false;

		nbytes = _read(m_fd, buffer, nsize);

		if (nbytes != nsize)
			return false;

		return true;
	}

};
