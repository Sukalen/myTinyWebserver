#ifndef STATIC_FILE_HANDLER_H
#define STATIC_FILE_HANDLER_H


#include<string>
#include<cstddef>

class StaticFileHandler
{
public:
	
	
	
	class MappedFile
	{
	public:
		MappedFile() = default;

		~MappedFile();

		MappedFile(const MappedFile&) = delete;
		MappedFile& operator=(const MappedFile&) = delete;

		MappedFile(MappedFile&& other) noexcept;
		MappedFile& operator=(MappedFile&& other) noexcept;

        void reset() noexcept;

        char* data() const noexcept
        {
            return m_address;
        }

        std::size_t size() const noexcept
        {
            return m_size;
        }

        bool empty() const noexcept
        {
            return 0 == m_size;
        }

	private:
		friend class StaticFileHandler;

		MappedFile(char* address, std::size_t size):m_address(address), m_size(size)
		{}

	private:
		char* m_address = nullptr;
		std::size_t m_size = 0;
	};



	enum class Result
	{
        Success,
        NotFound,
        Forbidden,
        BadRequest,
        InternalError
	};

	struct LoadResult
	{
		Result result = Result::InternalError;
		MappedFile file;
	};

public:
	explicit StaticFileHandler(std::string doc_root);

	LoadResult load(const std::string& target) const;

private:
	bool is_safe_target(const std::string& target) const;
	

private:
	std::string m_doc_root;

};

#endif
