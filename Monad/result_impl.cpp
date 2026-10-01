#include <result.hpp>

template <typename OutputType, typename Err>
bool Result<OutputType, Err>::isErr() const
{
    return isError;
}

template <typename OutputType, typename Err>
Err Result<OutputType, Err>::getErr() const
{
    return data;
}

template <typename OutputType, typename Err>
OutputType Result<OutputType, Err>::getResult() const
{
    return data;
}

template <typename OutputType, typename Err>
Result<OutputType, Err> Result<OutputType, Err>::from_err(Err err)
{
    return Result<OutputType, Err>(err, true);
}

template <typename OutputType, typename Err>
Result<OutputType, Err> Result<OutputType, Err>::from_output(OutputType output)
{
    return Result<OutputType, Err>(output, false);
}
