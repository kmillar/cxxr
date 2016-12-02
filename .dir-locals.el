((nil . ((eval . (add-hook 'before-save-hook
                           'whitespace-cleanup nil t))))
 (c++-mode . (
              (c-file-style . "stroustrup")
              (indent-tabs-mode . t)  ; use tabs rather than spaces
              (c-file-offsets . (
                                 (inline-open . 0)
                                 (access-label . /)
                                 ))))
 )
