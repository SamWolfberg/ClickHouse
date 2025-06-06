#pragma once

#include <base/types.h>
#include <Common/levenshteinDistance.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <queue>
#include <utility>

namespace DB
{

template <size_t MaxNumHints>
class NamePrompter
{
public:
    using DistanceIndex = std::pair<size_t, size_t>;
    using DistanceIndexQueue = std::priority_queue<DistanceIndex>;

    static std::vector<String> getHints(const String & name, const std::vector<String> & prompting_strings)
    {
        DistanceIndexQueue queue;
        for (size_t i = 0; i < prompting_strings.size(); ++i)
            appendToQueue(i, name, queue, prompting_strings);
        return release(queue, prompting_strings);
    }

private:
    static void appendToQueue(size_t ind, const String & name, DistanceIndexQueue & queue, const std::vector<String> & prompting_strings)
    {
        const String & prompt = prompting_strings[ind];

        /// Clang SimpleTypoCorrector logic
        const size_t min_possible_edit_distance = std::abs(static_cast<int64_t>(name.size()) - static_cast<int64_t>(prompt.size()));
        const size_t mistake_factor = (name.size() + 2) / 3;
        if (min_possible_edit_distance > 0 && name.size() / min_possible_edit_distance < 3)
            return;

        if (prompt.size() <= name.size() + mistake_factor && prompt.size() + mistake_factor >= name.size())
        {
            size_t distance = levenshteinDistanceCaseInsensitive(prompt, name);
            if (distance <= mistake_factor)
            {
                queue.emplace(distance, ind);
                if (queue.size() > MaxNumHints)
                    queue.pop();
            }
        }
    }

    static std::vector<String> release(DistanceIndexQueue & queue, const std::vector<String> & prompting_strings)
    {
        std::vector<String> answer;
        answer.reserve(queue.size());
        while (!queue.empty())
        {
            auto top = queue.top();
            queue.pop();
            answer.push_back(prompting_strings[top.second]);
        }
        std::reverse(answer.begin(), answer.end());
        return answer;
    }
};

String getHintsErrorMessageSuffix(const std::vector<String> & hints);

void appendHintsMessage(String & error_message, const std::vector<String> & hints);

template <size_t MaxNumHints = 1>
class IHints
{
public:
    virtual std::vector<String> getAllRegisteredNames() const = 0;

    std::vector<String> getHints(const String & name) const
    {
        return prompter.getHints(name, getAllRegisteredNames());
    }

    std::vector<String> getHints(const String & name, const std::vector<String> & prompting_strings) const
    {
        return prompter.getHints(name, prompting_strings);
    }

    void appendHintsMessage(String & error_message, const String & name) const
    {
        auto hints = getHints(name);
        DB::appendHintsMessage(error_message, hints);
    }

    String getHintsMessage(const String & name) const
    {
        return getHintsErrorMessageSuffix(getHints(name));
    }

    IHints() = default;

    IHints(const IHints &) = default;
    IHints(IHints &&) noexcept = default;
    IHints & operator=(const IHints &) = default;
    IHints & operator=(IHints &&) noexcept = default;

    virtual ~IHints() = default;

private:
    NamePrompter<MaxNumHints> prompter;
};
}
