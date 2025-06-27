// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/ComObject.h>

template<>
struct InterfacesOf<IStream> {
    using type = TypeList<IUnknown, ISequentialStream, IStream>;
};

class IStreamOnVector final : public ComObject<IStreamOnVector, IStream> {
    static_assert(sizeof(size_t) >= sizeof(ULONG));
    static_assert(sizeof(ULARGE_INTEGER) >= sizeof(size_t));

    static constexpr auto s_maxPos = size_t(std::numeric_limits<ULONG>::max());

public:
    IStreamOnVector(std::vector<uint8_t> & buf, size_t pos = 0):
        m_buf(buf),
        m_seekPos(pos)
    {}

    STDMETHODIMP Read(_Out_writes_bytes_to_(cb, *pcbRead) void * pv, _In_  ULONG cb, _Out_opt_  ULONG * pcbRead) override {
        COM_PROLOG
            size_t toRead = std::min(m_buf.size() - m_seekPos, size_t(cb));
            memcpy(pv, m_buf.data() + m_seekPos, toRead);
            m_seekPos += toRead;
            if (pcbRead)
                *pcbRead = ULONG(toRead);
            return toRead == cb ? S_OK : S_FALSE;
        COM_EPILOG
    }

    STDMETHODIMP Write(_In_reads_bytes_(cb)  const void * pv, _In_  ULONG cb, _Out_opt_  ULONG * pcbWritten) override {
        COM_PROLOG
            auto tail = m_buf.size() - m_seekPos;
            auto requested = size_t(cb); 
            if (requested > tail) {
                size_t increase = requested - tail;
                if (m_buf.size() > s_maxPos - increase)
                    return E_INVALIDARG;
                m_buf.resize(m_buf.size() + increase);
            }
            memcpy(m_buf.data() + m_seekPos, pv, requested);
            m_seekPos += requested;
            if (pcbWritten)
                *pcbWritten = ULONG(requested);
            return S_OK;
        COM_EPILOG
    }

    STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, _Out_opt_  ULARGE_INTEGER * plibNewPosition) override {
        COM_PROLOG
            
            if constexpr (sizeof(LARGE_INTEGER) > sizeof(LONG)) {
                if (dlibMove.QuadPart > LONGLONG(std::numeric_limits<LONG>::max()) ||
                    dlibMove.QuadPart < LONGLONG(std::numeric_limits<LONG>::min()))
                    return E_INVALIDARG;
            }

            auto offset = LONG(dlibMove.QuadPart);
            
            size_t newPos;
            if (dwOrigin == STREAM_SEEK_SET) {
                if (offset >= 0)
                    newPos = size_t(offset);
                else
                    newPos = 0;
            } else {
                size_t base;
                if (dwOrigin == STREAM_SEEK_CUR)
                    base = m_seekPos;
                else if (dwOrigin == STREAM_SEEK_END)
                    base = m_buf.size();
                else
                    return E_INVALIDARG;

                if (offset >= 0) {
                    size_t value = size_t(offset);
                    newPos = (base > s_maxPos - value) ? s_maxPos : base + value;
                } else {
                    size_t value = size_t(-offset);
                    newPos = base < value ? 0 : base - value;
                }
            }
            
            if (newPos > m_buf.size())
                m_buf.resize(newPos);
            m_seekPos = newPos;
            
            if (plibNewPosition)
                plibNewPosition->QuadPart = newPos;
            return S_OK;
        COM_EPILOG
    }

    STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize) override {
        COM_PROLOG
            if constexpr (sizeof(ULARGE_INTEGER) > sizeof(ULONG)) {
                if (libNewSize.QuadPart > ULONGLONG(s_maxPos))
                    return E_INVALIDARG;
            }

            m_buf.resize(size_t(libNewSize.QuadPart));
            if (m_seekPos > m_buf.size())
                m_seekPos = m_buf.size();
            return S_OK;
        COM_EPILOG
    }

    STDMETHODIMP CopyTo(_In_  IStream *pstm, ULARGE_INTEGER cb, 
                        _Out_opt_  ULARGE_INTEGER *pcbRead, _Out_opt_  ULARGE_INTEGER *pcbWritten) override {
        COM_PROLOG
            size_t toRead = std::min(m_buf.size() - m_seekPos, size_t(cb.QuadPart));
            size_t oldSeekPos = m_seekPos;
            m_seekPos += toRead; //must set it here in case Write operates on ourseves!
            ULONG written;
            comTest(pstm->Write(m_buf.data() + oldSeekPos, ULONG(toRead), &written));

            if (pcbRead)
                pcbRead->QuadPart = toRead;
            if (pcbWritten)
                pcbWritten->QuadPart = written;
            
            return S_OK;
        COM_EPILOG
    }

    STDMETHODIMP Commit(DWORD /*grfCommitFlags*/) override
        { return E_NOTIMPL; }

    STDMETHODIMP Revert() override 
        { return E_NOTIMPL; }

    STDMETHODIMP LockRegion(ULARGE_INTEGER /*libOffset*/, ULARGE_INTEGER /*cb*/, DWORD /*dwLockType*/) override
        { return E_NOTIMPL; }

    STDMETHODIMP UnlockRegion(ULARGE_INTEGER /*libOffset*/, ULARGE_INTEGER /*cb*/, DWORD /*dwLockType*/) override
        { return E_NOTIMPL; }

    STDMETHODIMP Stat(__RPC__out STATSTG */*pstatstg*/, DWORD /*grfStatFlag*/) override
        { return E_NOTIMPL; }

    STDMETHODIMP Clone(__RPC__deref_out_opt IStream **ppstm) override {
        COM_PROLOG
            *ppstm = new IStreamOnVector(m_buf, m_seekPos);
        return S_OK;
        COM_EPILOG
    }

private:
    std::vector<uint8_t> & m_buf;
    size_t m_seekPos = 0;
};