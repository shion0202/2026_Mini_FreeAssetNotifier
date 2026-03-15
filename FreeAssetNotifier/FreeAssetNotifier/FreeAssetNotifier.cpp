#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

struct AssetInfo {
    string name;
    string link;
    string coupon;
    string imageUrl;
};

// config.txt에서 여러 개의 웹훅 URL을 읽어오는 함수
vector<string> LoadWebhookUrls(const string& filename) {
    vector<string> urls;
    ifstream file(filename);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            // 앞뒤 공백 제거 (Trim)
            line.erase(0, line.find_first_not_of(" \n\r\t"));
            if (line.find_last_not_of(" \n\r\t") != string::npos) {
                line.erase(line.find_last_not_of(" \n\r\t") + 1);
            }

            // 빈 줄이 아니고 http로 시작하는 경우에만 추가
            if (!line.empty() && line.find("http") == 0) {
                urls.push_back(line);
            }
        }
        file.close();
    }
    return urls;
}

// 페이지 소스 다운로드 (Windows 내장 curl 사용)
bool DownloadPageSource(const string& url, const string& filename) {
    string command = "curl -s -L -A \"Mozilla/5.0\" \"" + url + "\" -o " + filename;
    int result = system(command.c_str());
    return (result == 0);
}

// HTML에서 에셋 이름, 링크, 쿠폰 코드를 추출하는 함수
AssetInfo ParseUnityAsset(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return {};

    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    AssetInfo info;
    // 기준점이 되는 문구 찾기
    string anchor = "ASSET GIVEAWAY";
    size_t anchorPos = content.find(anchor);
    if (anchorPos == string::npos) {
        anchor = "asset giveaway";
        anchorPos = content.find(anchor);
        if (anchorPos == string::npos) return info;
    }

    // 에셋 이름 추출 (<h2> 태그)
    size_t h2Start = content.find("<h2", anchorPos);
    if (h2Start != string::npos) {
        size_t nameStart = content.find(">", h2Start) + 1;
        size_t h2End = content.find("</h2>", nameStart);
        if (h2End != string::npos) {
            info.name = content.substr(nameStart, h2End - nameStart);
        }
    }

    // 에셋 페이지 링크 추출
    size_t linkTagPos = content.find("<a href=\"/packages/", anchorPos);
    if (linkTagPos != string::npos) {
        size_t hrefStart = content.find("href=\"", linkTagPos) + 6;
        size_t hrefEnd = content.find("\"", hrefStart);
        if (hrefEnd != string::npos) {
            string relativePath = content.substr(hrefStart, hrefEnd - hrefStart);
            info.link = "https://assetstore.unity.com" + relativePath;
        }
    }

    // 쿠폰 코드 추출
    regex couponRegex("coupon code ([A-Z0-9]+)");
    smatch match;
    auto searchStart = content.cbegin() + anchorPos;
    auto searchEnd = content.cend();

    if (regex_search(searchStart, searchEnd, match, couponRegex)) {
        info.coupon = match[1].str();
    }

    // 에셋 대표 이미지 URL 추출 (앵커보다 위쪽 섹션부터 검색)
    size_t sectionStart = content.rfind("<section data-type=\"CalloutSlim\"", anchorPos);
    if (sectionStart == string::npos) sectionStart = 0;

    regex imgTagRegex("<img([^>]+)>");
    smatch tagMatch;

    auto imgSearchStart = content.cbegin() + sectionStart;
    auto imgSearchEnd = content.cend();

    bool found = false;
    while (regex_search(imgSearchStart, imgSearchEnd, tagMatch, imgTagRegex)) {
        string tagContent = tagMatch[1].str();

        if (tagContent.find("object-cover") != string::npos) {
            regex srcRegex("src=\"([^\"]+)\"");
            smatch srcMatch;
            if (regex_search(tagContent, srcMatch, srcRegex)) {
                string rawUrl = srcMatch[1].str();

                if (rawUrl.find(".svg") == string::npos) {
                    if (rawUrl.find("//") == 0) info.imageUrl = "https:" + rawUrl;
                    else info.imageUrl = rawUrl;

                    found = true;
                    break;
                }
            }
        }
        imgSearchStart = tagMatch[0].second;

        // 기준점 근처까지만 검색하여 엉뚱한 이미지 방지
        if (static_cast<size_t>(distance(content.cbegin(), imgSearchStart)) > anchorPos + 1000) break;
    }

    if (!found) cout << "Asset Image not found in the target section." << endl;

    return info;
}

// 디스코드 웹훅 전송 (curl 사용)
void SendDiscordNotification(const string& webhookUrl, const AssetInfo& info) {
    if (webhookUrl.empty()) return;

    try {
        json payload;
        payload["content"] = "🔔 **새로운 기간 한정 무료 에셋**";

        json embed = json::object();
        embed["title"] = info.name;
        embed["url"] = info.link;
        embed["color"] = 2236962;

        embed["fields"] = json::array({
            { {"name", "🎁 쿠폰 코드"}, {"value", "`" + info.coupon + "`"}, {"inline", true} },
            { {"name", "🛒 스토어"}, {"value", "Unity Asset Store"}, {"inline", true} }
            });

        if (!info.imageUrl.empty()) {
            embed["image"] = { {"url", info.imageUrl} };
        }

        payload["embeds"] = json::array({ embed });

        string tempJsonFile = "temp_payload.json";
        ofstream o(tempJsonFile);
        if (o.is_open()) {
            o << payload.dump(-1, ' ', false, json::error_handler_t::replace);
            o.close();

            string command = "curl -s -H \"Content-Type: application/json\" -X POST -d @\"" + tempJsonFile + "\" " + webhookUrl;
            system(command.c_str());

            remove(tempJsonFile.c_str());
        }
    }
    catch (const json::exception& e) {
        cerr << "[JSON Error] " << e.what() << endl;
    }
}

int main() {
    const string configFileName = "config.txt";
    const string cacheFileName = "last_asset.txt";
    const string tempFileName = "unity_source.html";
    const string unityUrl = "https://assetstore.unity.com/ko-KR/publisher-sale";

    // 웹훅 URL 리스트 로드
    vector<string> webhookUrls = LoadWebhookUrls(configFileName);
    if (webhookUrls.empty()) {
        cout << "[Error] No Webhook URLs found in " << configFileName << endl;
        return 1;
    }

    cout << "Checking for updates..." << endl;

    if (DownloadPageSource(unityUrl, tempFileName)) {
        AssetInfo current = ParseUnityAsset(tempFileName);

        if (current.name.empty()) {
            cout << "[Error] Could not find asset info." << endl;
            return 1;
        }

        string lastAssetName = "";
        ifstream fin(cacheFileName);
        if (fin.is_open()) {
            getline(fin, lastAssetName);
            fin.close();
        }

        // 새로운 에셋 발견 시 모든 웹훅으로 전송
        if (current.name != lastAssetName) {
            cout << "New Asset: " << current.name << " (Coupon: " << current.coupon << ")" << endl;

            for (const string& url : webhookUrls) {
                cout << "Sending notification to a channel..." << endl;
                SendDiscordNotification(url, current);
            }

            ofstream fout(cacheFileName);
            if (fout.is_open()) {
                fout << current.name;
                fout.close();
            }
            cout << "All notifications sent successfully." << endl;
        }
        else {
            cout << "No updates. (Current: " << lastAssetName << ")" << endl;
        }
    }

    return 0;
}
