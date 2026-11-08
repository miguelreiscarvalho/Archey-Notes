#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tracos {
struct Box {
    double left, top, right, bottom;
    void include(double x, double y) {
        left = std::min(left, x); top = std::min(top, y);
        right = std::max(right, x); bottom = std::max(bottom, y);
    }
    bool near(const Box& other) const {
        // Same manuscript line, a following line, or a delayed accent.
        return other.left <= right + 120 && other.right >= left - 48 &&
               other.top <= bottom + 72 && other.bottom >= top - 48;
    }
    bool startsBelow(const Box& other, double renderedBottom, double fontSize) const {
        return other.top > renderedBottom + 2 &&
               (other.bottom-other.top > std::max(3.0,fontSize*.35) || other.top>renderedBottom+fontSize*.75);
    }
};
struct Word { std::string text; Box source; std::vector<size_t> ids; };
struct Line {
    Box source;
    double center;
    std::vector<Word> words;
    std::string text() const {
        std::string result;
        for (const auto& w:words) {
            if (!result.empty() && w.text.find_first_of(",.;:!?)]}") != 0) result+=' ';
            result+=w.text;
        }
        return result;
    }
};
inline std::vector<Line> composeLines(std::vector<Word> words) {
    if (words.empty()) return {};
    auto punctuation=[](const Word& word) { return word.text.find_first_not_of(",.;:!?") == std::string::npos; };
    std::vector<double> heights;
    for (const auto& w:words) if (!punctuation(w)) heights.push_back(w.source.bottom-w.source.top);
    if (heights.empty()) heights.push_back(14);
    std::sort(heights.begin(),heights.end());
    double tolerance=std::max(10.0,heights[heights.size()/2]*.7);
    std::stable_sort(words.begin(),words.end(),[](const auto& a,const auto& b) {
        return a.source.top+a.source.bottom < b.source.top+b.source.bottom;
    });
    std::vector<Line> lines;
    std::vector<Word> marks;
    for (auto& word:words) {
        if (punctuation(word)) { marks.push_back(std::move(word));continue; }
        double center=(word.source.top+word.source.bottom)/2;
        if (lines.empty() || center-lines.back().center>tolerance) lines.push_back({word.source,center,{}});
        auto& line=lines.back();line.source.include(word.source.left,word.source.top);
        line.source.include(word.source.right,word.source.bottom);
        line.words.push_back(std::move(word));
    }
    for (auto& mark:marks) {
        if (lines.empty()) lines.push_back({mark.source,(mark.source.top+mark.source.bottom)/2,{}});
        auto nearest=std::min_element(lines.begin(),lines.end(),[&](const auto& a,const auto& b) {
            double y=(mark.source.top+mark.source.bottom)/2;
            return std::abs(y-a.source.bottom)<std::abs(y-b.source.bottom);
        });
        // Punctuation belongs to a baseline; it must not create a spurious row below it.
        nearest->words.push_back(std::move(mark));
    }
    for (auto& line:lines) std::stable_sort(line.words.begin(),line.words.end(),[](const auto& a,const auto& b) {
        return a.source.left<b.source.left;
    });
    return lines;
}
inline std::string paragraph(std::vector<Word> words) {
    std::string text;
    for (const auto& line:composeLines(std::move(words))) {
        if (!text.empty()) text+='\n';
        text+=line.text();
    }
    return text;
}
} // namespace tracos
