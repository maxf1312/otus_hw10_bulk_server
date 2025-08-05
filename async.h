#pragma once

extern "C"
{
    typedef void* libasync_ctx_t;

    /// @brief Открывает сессию обработки команд. 
    /// @param bulk_size - размер блока.
    /// @return контекст (ид) сессии.
    libasync_ctx_t  connect(size_t bulk_size);

    /// @brief принимает команду (список команд, если встречается перевод строки). 
    /// @param ctx контекст 
    /// @param buf  указателя на начало буфера с текстом команд
    /// @param buf_sz размер буфера
    /// @return 0 - успешно, иначе код ошибки
    int receive(libasync_ctx_t ctx, const char buf[], size_t buf_sz);

    /// @brief Закрывает сессию и разрушает контекст. С точки зрения логики обработки команд этот вызов считается завершением текущего блока команд.
    /// @param ctx 
    /// @return 0 - успешно, иначе код ошибки
    int disconnect(libasync_ctx_t ctx);
};
