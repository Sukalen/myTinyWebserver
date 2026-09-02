#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>


#include "static_file_handler.h"
#include "../log/log.h"

namespace
{

class ScopedFd
{
public:
    explicit ScopedFd(int fd) : m_fd(fd)
    {
    }

    ~ScopedFd()
    {
        if(m_fd >= 0)
        {
            close(m_fd);
        }
    }

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    int get() const noexcept
    {
        return m_fd;
    }

private:
    int m_fd;
};


}


StaticFileHandler::MappedFile::~MappedFile()
{
	reset();
}

StaticFileHandler::MappedFile::MappedFile(MappedFile&& other) noexcept
				:m_address(other.m_address), m_size(other.m_size)
{
	other.m_address = nullptr;
	other.m_size = 0;
}

StaticFileHandler::MappedFile&
StaticFileHandler::MappedFile::operator=(MappedFile&& other) noexcept
{
    if(this == &other)
    {
        return *this;
    }

    reset();

    m_address = other.m_address;
    m_size = other.m_size;

    other.m_address = nullptr;
    other.m_size = 0;

    return *this;
}


void StaticFileHandler::MappedFile::reset() noexcept
{
    if(m_address && m_size > 0)
    {
        munmap(m_address, m_size);
    }

    m_address = nullptr;
    m_size = 0;
}

StaticFileHandler::StaticFileHandler(std::string doc_root)
    		: m_doc_root(std::move(doc_root))
{
    if(m_doc_root.empty())
    {
        throw std::invalid_argument("doc_root must not be empty");
    }

    if('/' == m_doc_root.back())
    {
        m_doc_root.pop_back();
    }
}


bool StaticFileHandler::is_safe_target(const std::string& target) const
{
    if(target.empty() ||
       target.front() != '/')
    {
        return false;
    }

    std::size_t begin = 1;

    while(begin <= target.size())
    {
        std::size_t end = target.find('/', begin);

        if(std::string::npos == end)
        {
            end = target.size();
        }

        const std::string segment =
            target.substr(begin, end - begin);

        if(".." == segment)
        {
            return false;
        }

        if(end == target.size())
        {
            break;
        }

        begin = end + 1;
    }

    return true;
}


StaticFileHandler::LoadResult
StaticFileHandler::load(const std::string& target) const
{
    LoadResult result;

    if(!is_safe_target(target))
    {
        result.result = Result::Forbidden;
        return result;
    }

    const std::string full_path = m_doc_root + target;

    struct stat file_stat{};

    if(stat(full_path.c_str(), &file_stat) < 0)
    {
        if(ENOENT == errno)
        {
            result.result = Result::NotFound;
        }
        else if(EACCES == errno)
        {
            result.result = Result::Forbidden;
        }
        else
        {
            result.result = Result::InternalError;
        }

        return result;
    }

    if(S_ISDIR(file_stat.st_mode))
    {
        result.result = Result::BadRequest;
        return result;
    }

    if(!(file_stat.st_mode & S_IROTH))
    {
        result.result = Result::Forbidden;
        return result;
    }

    if(0 == file_stat.st_size)
    {
        result.result = Result::Success;
        return result;
    }

	ScopedFd fd(open(full_path.c_str(),O_RDONLY));

    if(fd.get() < 0)
    {
        result.result =
            errno == EACCES ? Result::Forbidden : Result::InternalError;

        return result;
    }

    void* mapped =
        mmap(
            nullptr,
            static_cast<std::size_t>(file_stat.st_size),
            PROT_READ,
            MAP_PRIVATE,
            fd.get(),
            0);


    if(MAP_FAILED == mapped)
    {
        LOG_ERROR(
            "mmap failed: file=%s "
            "errno=%d error=%s",
            full_path.c_str(),
            errno,
            strerror(errno));

        result.result = Result::InternalError;

        return result;
    }

    result.file =
        MappedFile(
            static_cast<char*>(mapped),
            static_cast<std::size_t>(file_stat.st_size));

    result.result = Result::Success;
    return result;
}


