// UploadToNAS.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;
using namespace std::chrono;

// エラーが出たときにどういうエラーなのか特定しやすくするためにキー入力があるまで停止する
void error_stop()
{
    std::cerr << "The program has stopped due to the above error. Press any key to exit." << std::endl;
    std::cin.get();
}

// `YYYY-MM-DD HH:MM:SS` 形式の現在時刻を取得
std::string get_current_time_string()
{
    auto now = system_clock::to_time_t(system_clock::now());
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now); // Windows
#else
    localtime_r(&now, &tm); // Linux / macOS
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// `YYYY-MM-DD HH:MM:SS` の文字列を `fs::file_time_type` に変換
fs::file_time_type to_file_time(const std::string &timestamp)
{
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail())
    {
        std::cerr << "Failed to parse timestamp: " << timestamp << std::endl;
        std::cerr << "Expected format: YYYY-MM-DD HH:MM:SS" << std::endl;
        error_stop();
        exit(1);
    }
    auto time_c = std::mktime(&tm);
    if (time_c == -1) {
        std::cerr << "Failed to convert timestamp using mktime()" << std::endl;
        error_stop();
        exit(1);
    }
    auto time_point = system_clock::from_time_t(time_c);
    return clock_cast<fs::file_time_type::clock>(time_point);
}

// `YYYY/yyyymmddhhmm` 形式のフォルダ名を作成
std::string get_timestamp_folder()
{
    auto now = system_clock::to_time_t(system_clock::now());
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y/%Y%m%d%H%M");
    return oss.str();
}

// 現在時刻を `last_run.txt` に保存
void save_current_time(std::string source_path, std::string save_path)
{
    std::cout << "Overwriting last_run.txt." << std::endl;
    std::string str_date_time = get_current_time_string();
    std::ofstream file("last_run.txt");
    if (file.is_open())
    {
        file << str_date_time << "\n"
             << source_path << "\n"
             << save_path << "\n";
    }
    else
    {
        std::cerr << "There were some problems opening the last_run.txt file." << std::endl;
        error_stop();
        return;
    }
}

void copy_recent_files(const fs::path &srcDir, const fs::path &baseDestDir, const std::string &timestamp)
{
    // `last_run.txt` から前回の実行時間を取得
    fs::file_time_type cutoff_time = to_file_time(timestamp);

    // `YYYY/yyyymmddhhmm` のフォルダ用パスを作成
    std::string timestamp_folder = get_timestamp_folder();
    fs::path destDir = baseDestDir / timestamp_folder;

    bool copied = false;

    for (const auto &entry : fs::recursive_directory_iterator(srcDir))
    {
        if (fs::is_regular_file(entry))
        {
            fs::file_time_type last_write_time = fs::last_write_time(entry);

            if (last_write_time > cutoff_time)
            {
                if (!copied) // 変更や追加されたファイルがあった場合のみ `YYYY/yyyymmddhhmm` のフォルダを作成
                {
                    if (!fs::exists(destDir))
                    {
                        try {
                            fs::create_directories(destDir);
                        } catch (const fs::filesystem_error& e) {
                            std::cerr << "Failed to create backup directory: " << destDir << std::endl;
                            std::cerr << "Error: " << e.what() << std::endl;
                            error_stop();
                            exit(1);
                        }
                    }
                }
                fs::path relativePath = fs::relative(entry.path(), srcDir);
                fs::path destPath = destDir / relativePath;

                try {
                    fs::create_directories(destPath.parent_path());
                } catch (const fs::filesystem_error& e) {
                    std::cerr << "Failed to create destination directory: " << destPath.parent_path() << std::endl;
                    std::cerr << "Error: " << e.what() << std::endl;
                    error_stop();
                    exit(1);
                }

                try {
                    fs::copy_file(entry, destPath, fs::copy_options::overwrite_existing);
                    copied = true;
                    std::cout << "Copied: " << entry.path() << " -> " << destPath << std::endl;
                } catch (const fs::filesystem_error& e) {
                    std::cerr << "Failed to copy file:" << std::endl;
                    std::cerr << "Source: " << entry.path() << std::endl;
                    std::cerr << "Destination: " << destPath << std::endl;
                    std::cerr << "Error: " << e.what() << std::endl;
                    error_stop();
                    exit(1);
                }
            }
        }
    }

    if (copied)
    {
        try {
            save_current_time(srcDir.string(), baseDestDir.string()); // コピーが発生した場合のみ `last_run.txt` を更新
        } catch (const std::exception& e) {
            std::cerr << "Failed to update last_run.txt" << std::endl;
            std::cerr << "Error: " << e.what() << std::endl;
            error_stop();
            exit(1);
        }
    }
    else
    {
        std::cout << "There is no new file." << std::endl;
    }
}

void initialize_settings()
{
    std::string source_path, save_path;
    std::cout << "Performing initial setup." << std::endl;
    std::cout << "Please enter the path of the folder to back up." << std::endl;
    std::getline(std::cin, source_path);
    std::cout << "Please enter the path where the backup will be saved." << std::endl;
    std::getline(std::cin, save_path);
    save_current_time(source_path, save_path);
}

int main(int argc, char *argv[])
{
    std::filesystem::path filePath = "last_run.txt";

    if (std::filesystem::exists(filePath))
    {
        std::cout << "Loading last_run.txt." << std::endl;

        std::ifstream file("last_run.txt");
        if (file)
        {
            std::string source_path, save_path, last_run_time;
            // ファイルから値を読み込んで変数に格納
            std::getline(file, last_run_time);
            std::getline(file, source_path);
            std::getline(file, save_path);

            file.close(); // ファイルを閉じる

            // 読み込んだ内容を表示
            std::cout << "Previous execution time: " << last_run_time << std::endl;

            fs::path fs_source_path = source_path;
            fs::path fs_save_path = save_path;

            std::cout << "Source copy: " << fs_source_path << std::endl;
            std::cout << "Destination: " << fs_save_path << std::endl;

            // コピー元とコピー先のファイルが存在することを確認する。
            if (!fs::exists(fs_source_path) || !fs::is_directory(fs_source_path))
            {
                std::cerr << "Error: The source folder does not exist." << std::endl;
                error_stop();
                return 1;
            }
            if (!fs::exists(fs_save_path))
            {
                std::cerr << "Error: The destination folder does not exist. Please check the connection to the NAS." << std::endl;
                error_stop();
                return 1;
            }

            copy_recent_files(fs_source_path, fs_save_path, last_run_time);
        }
        else
        {
            std::cerr << "Failed to open the file." << std::endl;
        }
    }
    else
    {
        std::cout << "last_run.txt does not exist." << std::endl;
        initialize_settings();
    }
    return 0;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント:
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します