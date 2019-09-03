require 'asciidoctor'
require 'asciidoctor/extensions'

module Git
  module Documentation
    class LinkGitProcessor < Asciidoctor::Extensions::InlineMacroProcessor
      use_dsl

      named :chrome

      def process(parent, target, attrs)
        if parent.document.basebackend? 'html'
          prefix = parent.document.attr('git-relative-html-prefix')
          %(<a href="#{prefix}#{target}.html">#{target}(#{attrs[1]})</a>)
        elsif parent.document.basebackend? 'docbook'
          "<citerefentry>\n" \
            "<refentrytitle>#{target}</refentrytitle>" \
            "<manvolnum>#{attrs[1]}</manvolnum>\n" \
          "</citerefentry>"
        end
      end
    end

    class DocumentPostProcessor < Asciidoctor::Extensions::Postprocessor
      def process document, output
        if document.basebackend? 'docbook'
          git_version = document.attributes['git_version']
          new_tags = "" \
            "<refmiscinfo class=\"source\">Git</refmiscinfo>\n" \
            "<refmiscinfo class=\"version\">#{git_version}</refmiscinfo>\n" \
            "<refmiscinfo class=\"manual\">Git Manual</refmiscinfo>\n"
          output = output.sub(/<\/refmeta>/, new_tags + "</refmeta>")
        end
        output
      end
    end
  end
end

Asciidoctor::Extensions.register do
  inline_macro Git::Documentation::LinkGitProcessor, :linkgit
  postprocessor Git::Documentation::DocumentPostProcessor
end
