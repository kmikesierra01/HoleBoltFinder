#include "pch.h"
#include "MainWindow.h"
#include <QtWidgets/QApplication>
#include <QFile>
#include <QTextStream>
#include <QSurfaceFormat>
#include <Windows.h>
#include <intrin.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")

// Returns true if software OpenGL should be used.
// Logic: VM/Sandbox → software, real machine with hardware GPU → hardware.
static bool needsSoftwareOpenGL()
{
    // Check for hypervisor via CPUID ECX bit 31
    int cpu[4] = {};
    __cpuid(cpu, 1);
    if (cpu[2] & (1 << 31))
    {
        // Read hypervisor vendor (CPUID leaf 0x40000000)
        __cpuid(cpu, 0x40000000);
        char vendor[13] = {};
        memcpy(vendor,     &cpu[1], 4);
        memcpy(vendor + 4, &cpu[2], 4);
        memcpy(vendor + 8, &cpu[3], 4);

        // VMware / VirtualBox / KVM / Xen → always software
        if (strcmp(vendor, "VMwareVMware") == 0 ||
            strcmp(vendor, "VBoxVBoxVBox") == 0 ||
            strcmp(vendor, "KVMKVMKVM\0\0\0") == 0 ||
            strcmp(vendor, "XenVMMXenVMM") == 0)
            return true;

        // "Microsoft Hv": physical machine with Hyper-V/WSL2 OR Windows Sandbox.
        // Distinguish via username — Sandbox always runs as WDAGUtilityAccount.
        if (strcmp(vendor, "Microsoft Hv") == 0)
        {
            wchar_t username[256] = {};
            DWORD len = 256;
            if (GetUserNameW(username, &len) &&
                wcscmp(username, L"WDAGUtilityAccount") == 0)
                return true;  // Windows Sandbox
        }

        // else: physical machine with Hyper-V enabled → fall through to GPU check
    }

    // Check for hardware GPU via DXGI 1.1 (skips WARP / Basic Render Driver)
    bool hasGpu = false;
    if (HMODULE dll = LoadLibraryW(L"dxgi.dll"))
    {
        using Fn = HRESULT(WINAPI*)(REFIID, void**);
        if (auto fn = reinterpret_cast<Fn>(GetProcAddress(dll, "CreateDXGIFactory1")))
        {
            IDXGIFactory1* factory = nullptr;
            if (SUCCEEDED(fn(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
            {
                IDXGIAdapter1* adapter = nullptr;
                for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
                {
                    DXGI_ADAPTER_DESC1 desc{};
                    adapter->GetDesc1(&desc);
                    adapter->Release();
                    if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                        !wcsstr(desc.Description, L"Basic Render Driver"))
                    { hasGpu = true; break; }
                }
                factory->Release();
            }
        }
        FreeLibrary(dll);
    }
    return !hasGpu;
}

int main(int argc, char *argv[])
{
    // -- Set log file path based on exe directory -----------
    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();

    auto nowTt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm ltm{};
    localtime_s(&ltm, &nowTt);

    char monthBuf[8], fileBuf[32];
    strftime(monthBuf, sizeof(monthBuf), "%Y-%m", &ltm);
    strftime(fileBuf, sizeof(fileBuf), "%Y-%m-%d_%H-%M-%S", &ltm);

    std::filesystem::path logDir = exeDir / "log" / monthBuf;
    std::filesystem::create_directories(logDir);
    std::filesystem::path logFile = logDir / (std::string(fileBuf) + ".txt");

    // -- Multi-sink logger (console + file) -----------------
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile.string(), true);

    const std::string kPattern = "[%Y-%m-%d %H:%M:%S.%e] %v";
    consoleSink->set_pattern(kPattern);
    fileSink->set_pattern(kPattern);

    auto logger = std::make_shared<spdlog::logger>("console",
        spdlog::sinks_init_list{consoleSink, fileSink});
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);

    // Select OpenGL renderer: respects QT_OPENGL env var, otherwise auto-detects.
    {
        QByteArray glEnv = qgetenv("QT_OPENGL");
        if (glEnv.isEmpty())
        {
            if (needsSoftwareOpenGL())
            {
                qputenv("QT_OPENGL", "software");
                QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
                spdlog::info("[GL] Software renderer (VM/Sandbox or no hardware GPU)");
            }
            else
            {
                spdlog::info("[GL] Hardware renderer");
            }
        }
        else
        {
            if (glEnv == "software")
                QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
            spdlog::info("[GL] QT_OPENGL={} (from environment)", glEnv.toStdString());
        }
    }

    // Always request OpenGL with depth buffer explicitly
    {
        QSurfaceFormat fmt;
        fmt.setDepthBufferSize(24);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    QApplication a(argc, argv);

    QFile qssFile(":/theme.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&qssFile);
        a.setStyleSheet(ts.readAll());
        spdlog::info("[General] Theme stylesheet loaded OK");
    } else {
        spdlog::warn("[General] Failed to load theme.qss from resources");
    }

    a.setWindowIcon(QIcon(":/HoleBoltFinder/icons/HoleBoltFinderIcon.png"));

    MainWindow w;
    w.show();
    spdlog::info("[General] Program started");
    int ret = a.exec();

    spdlog::shutdown();
    return ret;
}

