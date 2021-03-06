#include <Parsers/ASTSettingsProfileElement.h>
#include <Common/FieldVisitors.h>
#include <Common/quoteString.h>


namespace DB
{
namespace
{
    void formatProfileNameOrID(const String & str, bool is_id, const IAST::FormatSettings & settings)
    {
        if (is_id)
        {
            settings.ostr << (settings.hilite ? IAST::hilite_keyword : "") << "ID" << (settings.hilite ? IAST::hilite_none : "") << "("
                          << quoteString(str) << ")";
        }
        else
        {
            settings.ostr << backQuoteIfNeed(str);
        }
    }
}

void ASTSettingsProfileElement::formatImpl(const FormatSettings & settings, FormatState &, FormatStateStacked) const
{
    if (!parent_profile.empty())
    {
        settings.ostr << (settings.hilite ? IAST::hilite_keyword : "") << "PROFILE " << (settings.hilite ? IAST::hilite_none : "");
        formatProfileNameOrID(parent_profile, id_mode, settings);
        return;
    }

    settings.ostr << name;

    if (!value.isNull())
    {
        settings.ostr << " = " << applyVisitor(FieldVisitorToString{}, value);
    }

    if (!min_value.isNull())
    {
        settings.ostr << (settings.hilite ? IAST::hilite_keyword : "") << " MIN " << (settings.hilite ? IAST::hilite_none : "")
                      << applyVisitor(FieldVisitorToString{}, min_value);
    }

    if (!max_value.isNull())
    {
        settings.ostr << (settings.hilite ? IAST::hilite_keyword : "") << " MAX " << (settings.hilite ? IAST::hilite_none : "")
                      << applyVisitor(FieldVisitorToString{}, max_value);
    }

    if (readonly)
    {
        settings.ostr << (settings.hilite ? IAST::hilite_keyword : "") << (*readonly ? " READONLY" : " WRITABLE")
                      << (settings.hilite ? IAST::hilite_none : "");
    }
}


bool ASTSettingsProfileElements::empty() const
{
    for (const auto & element : elements)
        if (!element->empty())
            return false;
    return true;
}


void ASTSettingsProfileElements::formatImpl(const FormatSettings & settings, FormatState &, FormatStateStacked) const
{
    if (empty())
    {
        settings.ostr << (settings.hilite ? IAST::hilite_keyword : "") << "NONE" << (settings.hilite ? IAST::hilite_none : "");
        return;
    }

    bool need_comma = false;
    for (const auto & element : elements)
    {
        if (need_comma)
            settings.ostr << ", ";
        need_comma = true;

        element->format(settings);
    }
}

}
