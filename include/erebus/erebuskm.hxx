#pragma once

#include <erebus/erebus.hxx>
#include <erebus/util/autoptr.hxx>
#include <erebus/util/generichandle.hxx>

#include <ctime>
#include <mutex>
#include <vector>

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef EREBUSKM_EXPORTS
        #define EREBUSKM_EXPORT __declspec(dllexport)
    #else
        #define EREBUSKM_EXPORT __declspec(dllimport)
    #endif
#else
    #define EREBUSKM_EXPORT __attribute__((visibility("default")))
#endif

namespace Erk
{

constexpr const char* ErebusDeviceName = "erebus";


EREBUSKM_EXPORT std::string_view getRootDir() noexcept;

class EREBUSKM_EXPORT ErebusDriver final
{
public:
    struct Task
    {
        uint64_t pid;
        clock_t user;
        clock_t system;

        Task(uint64_t pid = uint64_t(-1), uint64_t utime = 0, uint64_t stime = 0)
            : pid(pid)
            , user(clock_t(utime))
            , system(clock_t(stime))
        {}
    };

    ~ErebusDriver();
    explicit ErebusDriver(unsigned devno);

    template <typename Visitor>
        requires std::is_invocable_r_v<bool, Visitor, Task>
    size_t enumerateTasks(Visitor v)
    {
        std::unique_lock l(m_mutex);
        enumerateRawTasks();
        for (size_t index = 0; index < m_tasks->count; ++index)
        {
            auto& t = m_tasks->entries[index];
            if (!v(Task(t.pid, t.utime, t.stime)))
                break;
        }

        return m_tasks->count;
    }

private:
    struct FdCloser
    {
        void operator()(int fd) noexcept { ::close(fd); }
    };

    #pragma pack(push, 1)

    struct RawTask
    {
        uint64_t pid = uint64_t(-1);
        uint64_t utime = 0;
        uint64_t stime = 0;
    };

    struct RawTaskList 
    {
        int64_t count;
        int64_t limit;
        RawTask entries[0];

        constexpr explicit RawTaskList(int64_t limit) noexcept
            : limit(limit)
        {}
    };

    #pragma pack(pop)

    template <typename T>
    struct Deleter
    {
        void operator()(T* ptr) noexcept { ::free(ptr); }
    };

    static std::string makeDevicePath(unsigned devno);
    void enumerateRawTasks();

    Er::Util::GenericHandle<int, int, -1, FdCloser> m_fd;
    std::mutex m_mutex;
    Er::Util::AutoPtr<RawTaskList, Deleter<RawTaskList>> m_tasks;
};

} // namespace Erk {}
