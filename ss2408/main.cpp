#include <omp.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "mip.h"
#include "path.h"

int main(int argc, char* argv[]) {
    double start_time = omp_get_wtime();

    if (argc != 3) {
        std::cerr << "使用方法: " << argv[0] << " <出力ファイル> <入力テキストファイル>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string output = argv[1];     // 出力ファイル
    const std::string text_file = argv[2];  // 入力テキストファイル

    try {
        Path path;

        // テキストファイルの読み込み
        path.load_text_file(text_file);
        const auto& text_info = path.get_text_info();
        std::string mhd_file_name = text_info.at("Input") + ".mhd";
        std::string raw_file_name = text_info.at("Input") + ".raw";

        // ファイルパスの取得
        std::string dir_path = text_file;
        dir_path = dir_path.substr(0, dir_path.find_last_of('/') + 1);
        if (dir_path.empty()) {
            throw std::runtime_error("ディレクトリパスが取得できません");
        }
        std::string mhd_file = dir_path + mhd_file_name;
        std::string raw_file = dir_path + raw_file_name;

        // MHDファイルの読み込み
        path.load_mhd_file(mhd_file);
        const auto& mhd_info = path.get_mhd_info();

        // 画像サイズの取得
        const int width = std::stoi(mhd_info.at("DimSizeX"));
        const int height = std::stoi(mhd_info.at("DimSizeY"));
        const int depth = std::stoi(mhd_info.at("DimSizeZ"));
        const double ElementSpacingX = std::stod(mhd_info.at("ElementSpacingX"));
        const double ElementSpacingY = std::stod(mhd_info.at("ElementSpacingY"));
        const double ElementSpacingZ = std::stod(mhd_info.at("ElementSpacingZ"));
        std::map<std::string, double> spacing;
        spacing["ElementSpacingX"] = ElementSpacingX;
        spacing["ElementSpacingY"] = ElementSpacingY;
        spacing["ElementSpacingZ"] = ElementSpacingZ;
        const size_t data_size = width * height * depth;

        // RAWデータの読み込み
        if (raw_file.empty()) {
            throw std::runtime_error("RAWファイルが指定されていません");
        }
        std::vector<unsigned char> raw_data = path.load_raw_file(raw_file, data_size);

        // MIP画像の角度を取得
        const double phi = std::stod(text_info.at("Phi"));
        const double theta = std::stod(text_info.at("Theta"));
        const double psi = std::stod(text_info.at("Psi"));
        const EulerAngles angles(phi, theta, psi);

        // WindowParametersの設定
        WindowParameters window_params(false, 0, 0);  // デフォルト値で初期化
        if (text_info.at("WindowProcessing") == "True") {
            window_params =
                WindowParameters(true, std::stoi(text_info.at("WindowLevel")), std::stoi(text_info.at("WindowWidth")));
        }

        // MIP画像の生成
        std::vector<unsigned char> mip_image =
            EulerAngles::generate_mip_image(raw_data, width, height, depth, angles, spacing, window_params);

        // 結果の保存
        path.save_raw_file(output + ".raw", mip_image);

        auto new_mhd_info = mhd_info;  // 新しいmapを作成
        const int mip_size = std::sqrt(mip_image.size());
        new_mhd_info["DimSize"] = std::to_string(mip_size) + " " + std::to_string(mip_size) + " 1";
        new_mhd_info["ElementDataFile"] = output;
        new_mhd_info["ElementType"] = "MET_UCHAR";
        path.save_mhd_file(output + ".mhd", new_mhd_info);

        double end_time = omp_get_wtime();
        std::cout << "処理時間: " << end_time - start_time << "秒" << std::endl;
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
