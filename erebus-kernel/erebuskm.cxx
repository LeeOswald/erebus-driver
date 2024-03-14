#include <erebus/erebuskm.hxx>

#include <erebus/exception.hxx>
#include <erebus/util/format.hxx>
#include <erebus/util/posixerror.hxx>

#include <fcntl.h>
#include <sys/ioctl.h>


#define ERK_IOCTL_MAGIC              'e'

#define ERK_IOCTL_GET_PROCESS_LIST    _IO(ERK_IOCTL_MAGIC, 1)


namespace Erk
{

EREBUSKM_EXPORT std::string_view getRootDir() noexcept
{
    static auto dir = std::getenv("EREBUS_ROOT_DIR");
    static std::string_view str(dir ? dir : "");

    return str;
}


ErebusDriver::~ErebusDriver()
{
}

ErebusDriver::ErebusDriver(unsigned devno)
    : m_fd(::open(makeDevicePath(devno).c_str(), O_RDWR | O_SYNC))
{
    if (!m_fd.valid()) [[unlikely]]
    {
        auto e = errno;
        throw Er::Exception(ER_HERE(), Er::Util::format("Failed to open %s", makeDevicePath(devno).c_str()), Er::ExceptionProps::PosixErrorCode(e), Er::ExceptionProps::DecodedError(Er::Util::posixErrorToString(e)));
    }

    if (::fcntl(m_fd, F_SETFD, FD_CLOEXEC) == -1) 
    {
        auto e = errno;
        throw Er::Exception(ER_HERE(), Er::Util::format("Failed to set FD_CLOEXEC on %s", makeDevicePath(devno).c_str()), Er::ExceptionProps::PosixErrorCode(e), Er::ExceptionProps::DecodedError(Er::Util::posixErrorToString(e)));
    }
}

std::string ErebusDriver::makeDevicePath(unsigned devno)
{
    std::string path(getRootDir());
    path.append("/dev/");
    path.append(ErebusDeviceName);
    path.append(std::to_string(devno));

    return path;
}

void ErebusDriver::enumerateRawTasks()
{
    size_t required = 512;

    do
    {
        // allocate IOCTL request
        if (!m_tasks || m_tasks->limit < required) [[likely]] 
        {
            m_tasks.reset(static_cast<RawTaskList*>(::malloc(sizeof(RawTaskList) + required * sizeof(RawTask))));
            if (!m_tasks) [[unlikely]] 
            {
                throw Er::Exception(ER_HERE(), "Not enough memory");
            }

            new (m_tasks.get()) RawTaskList(required);
        }

        // repeat until we have enough out buffer room
        auto res = ::ioctl(m_fd, ERK_IOCTL_GET_PROCESS_LIST, m_tasks.get());
        if (res != 0) [[unlikely]]
        {
            if (errno != ENOSPC) [[unlikely]]
            {
                auto e = errno;
                throw Er::Exception(ER_HERE(), "ERK_IOCTL_GET_PROCESS_LIST failed", Er::ExceptionProps::PosixErrorCode(e), Er::ExceptionProps::DecodedError(Er::Util::posixErrorToString(e)));
            }

            required += 256;
        }
        else
        {
            break;
        }

    } while (true);
}


} // namespace Erk {}