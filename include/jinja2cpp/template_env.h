#ifndef JINJA2CPP_TEMPLATE_ENV_H
#define JINJA2CPP_TEMPLATE_ENV_H

#include "error_info.h"

#include "filesystem_handler.h"
#include "template.h"

#include <unordered_map>

namespace jinja2
{

class IErrorHandler;
class IFilesystemHandler;

class TemplateEnv
{
public:
    void SetErrorHandler(IErrorHandler* h)
    {
        m_errorHandler = h;
    }
    auto GetErrorHandler() const
    {
        return m_errorHandler;
    }
    void AddFilesystemHandler(std::string prefix, FilesystemHandlerPtr h)
    {
        m_filesystemHandlers.push_back(FsHandler{std::move(prefix), h});
    }
    void AddFilesystemHandler(std::string prefix, IFilesystemHandler& h)
    {
        m_filesystemHandlers.push_back(FsHandler{std::move(prefix), std::shared_ptr<IFilesystemHandler>(&h, [](auto*) {})});
    }
    nonstd::expected<Template, ErrorInfo> LoadTemplate(std::string fileName);
    nonstd::expected<TemplateW, ErrorInfoW> LoadTemplateW(std::string fileName);

private:
    IErrorHandler* m_errorHandler;
    struct FsHandler
    {
        std::string prefix;
        FilesystemHandlerPtr handler;
    };
    std::vector<FsHandler> m_filesystemHandlers;
};

} // jinja2

#endif // JINJA2CPP_TEMPLATE_ENV_H
