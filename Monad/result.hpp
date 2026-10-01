#ifndef _RESULT_MONAD
#define _RESULT_MONAD 1

template <typename OutputType, typename Err>
class Result
{
    bool isError;

    union DataPayload
    {
        OutputType output;
        Err err;
    } data;

    Result(DataPayload data, bool isErr = true) : data(data), isError(isErr) {}
public:
    bool isErr() const;
    Err getErr() const;
    OutputType getResult() const;

    static inline Result from_err(Err err);
    static inline Result from_output(OutputType output);
    ~Result(){};
};

#endif